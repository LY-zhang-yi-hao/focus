"""Focus Dial（专注拨盘）HA 自定义组件：

1) 专注统计写入 `.storage`（长期累加“总学习时长”）
2) 设备选择 YES 回写 TickTick 完成后：自动刷新 TickTick 列表并推送到设备
3) 推送 payload 同时包含待办/已完成（设备端可切换查看）
"""

from __future__ import annotations

import asyncio
import logging
from datetime import date
from typing import Any

import async_timeout
import voluptuous as vol
from aiohttp import ClientError, web

from homeassistant.components import webhook
from homeassistant.const import CONF_NAME
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.util import dt as dt_util

from .const import (
    CONF_DEFAULT_DURATION,
    CONF_DEVICE_HOST,
    CONF_MAX_COMPLETED,
    CONF_MAX_PENDING,
    CONF_TICKTICK_PROJECT_ID,
    CONF_TODO_ENTITY_ID,
    CONF_WEBHOOK_ID,
    DEFAULT_DURATION_MINUTES,
    DEFAULT_MAX_COMPLETED,
    DEFAULT_MAX_PENDING,
    DEFAULT_WEBHOOK_ID,
    DOMAIN,
)
from .storage import FocusDialStatsStore

_LOGGER = logging.getLogger(__name__)

SERVICE_PUSH_TASKS = "push_tasks"

CONFIG_SCHEMA = vol.Schema(
    {
        DOMAIN: vol.Schema(
            {
                vol.Optional(CONF_NAME, default="Focus Dial"): cv.string,
                vol.Required(CONF_DEVICE_HOST): cv.string,
                vol.Required(CONF_TODO_ENTITY_ID): cv.entity_id,
                vol.Required(CONF_TICKTICK_PROJECT_ID): cv.string,
                vol.Optional(CONF_WEBHOOK_ID, default=DEFAULT_WEBHOOK_ID): cv.string,
                vol.Optional(CONF_DEFAULT_DURATION, default=DEFAULT_DURATION_MINUTES): vol.Coerce(int),
                vol.Optional(CONF_MAX_PENDING, default=DEFAULT_MAX_PENDING): vol.Coerce(int),
                vol.Optional(CONF_MAX_COMPLETED, default=DEFAULT_MAX_COMPLETED): vol.Coerce(int),
            }
        )
    },
    extra=vol.ALLOW_EXTRA,
)


def _tasklist_url(device_host: str) -> str:
    base = device_host.strip().rstrip("/")
    if not base.startswith(("http://", "https://")):
        base = f"http://{base}"
    return f"{base}/api/tasklist"


async def _async_call_service_return_response(
    hass: HomeAssistant,
    domain: str,
    service: str,
    service_data: dict[str, Any] | None = None,
    target: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    """兼容不同 HA 版本：尽量拿到 service response。"""

    data = service_data or {}
    try:
        return await hass.services.async_call(
            domain,
            service,
            data,
            target=target,
            blocking=True,
            return_response=True,
        )
    except TypeError:
        await hass.services.async_call(domain, service, data, target=target, blocking=True)
        return None


def _extract_todo_items(resp: dict[str, Any] | None, todo_entity_id: str) -> list[dict[str, Any]]:
    if not isinstance(resp, dict):
        return []
    if todo_entity_id in resp and isinstance(resp[todo_entity_id], dict):
        items = resp[todo_entity_id].get("items")
        return items if isinstance(items, list) else []
    items = resp.get("items")
    return items if isinstance(items, list) else []


async def async_setup(hass: HomeAssistant, config: dict[str, Any]) -> bool:
    """通过 YAML 配置初始化集成。"""

    if DOMAIN not in config:
        return True

    domain_cfg = config[DOMAIN]
    name: str = domain_cfg.get(CONF_NAME, "Focus Dial")
    webhook_id: str = domain_cfg[CONF_WEBHOOK_ID]
    device_host: str = domain_cfg[CONF_DEVICE_HOST]
    todo_entity_id: str = domain_cfg[CONF_TODO_ENTITY_ID]
    ticktick_project_id: str = domain_cfg[CONF_TICKTICK_PROJECT_ID]
    default_duration: int = int(domain_cfg[CONF_DEFAULT_DURATION])
    max_pending: int = int(domain_cfg[CONF_MAX_PENDING])
    max_completed: int = int(domain_cfg[CONF_MAX_COMPLETED])

    stats_store = FocusDialStatsStore(hass)
    await stats_store.async_load()

    session = async_get_clientsession(hass)
    url_tasklist = _tasklist_url(device_host)
    push_lock = asyncio.Lock()

    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN]["config"] = domain_cfg
    hass.data[DOMAIN]["stats_store"] = stats_store

    async def _async_push_tasks() -> None:
        """刷新 TickTick 列表并推送到设备（含待办/本地缓存的已完成）。"""

        async with push_lock:
            pending_items: list[dict[str, Any]] = []

            # 先检查实体是否存在
            state = hass.states.get(todo_entity_id)
            if state is None:
                _LOGGER.error("Todo 实体不存在：%s，请检查配置", todo_entity_id)
                return

            try:
                # 获取待办任务（TickTick API 只返回待办）
                # Get pending tasks (TickTick API only returns pending)
                pending_resp = await hass.services.async_call(
                    "todo",
                    "get_items",
                    service_data={"status": ["needs_action"]},
                    target={"entity_id": todo_entity_id},
                    blocking=True,
                    return_response=True,
                )
                _LOGGER.debug("todo.get_items (pending) 返回：%s", pending_resp)
                pending_items = _extract_todo_items(pending_resp, todo_entity_id)
            except TypeError:
                # 旧版 HA 不支持 return_response
                _LOGGER.warning("当前 HA 版本可能不支持 service response，尝试从实体属性读取")
            except Exception as err:  # noqa: BLE001 - 外部依赖失败需兜底
                _LOGGER.warning("获取 todo 列表失败：%s，尝试从实体属性读取", err)

            # 兼容：从实体属性兜底读取
            if not pending_items:
                state = hass.states.get(todo_entity_id)
                if state is not None:
                    attr_items = state.attributes.get("items")
                    if isinstance(attr_items, list):
                        pending_items = attr_items

            stats_store.stats.ensure_today()
            today_tasks = stats_store.stats.today_tasks

            tasks: list[dict[str, Any]] = []
            pending_count = 0
            completed_count = 0

            cached_completed = stats_store.get_completed_tasks()

            # TickTick 的 todo 实体刷新可能存在延迟：回写完成后短时间内，todo.get_items 仍可能把任务当作“待办”返回。
            # 为了实现“点 YES 立刻从待办消失并进入已完成”，仅对“近期完成”（默认近 2 天）的任务做本地过滤。
            recent_completed_ids: set[str] = set()
            today = dt_util.now().date()
            for item in cached_completed:
                if not isinstance(item, dict):
                    continue
                uid = str(item.get("id") or "")
                if not uid:
                    continue

                completed_at = str(item.get("completed_at") or "")
                if not completed_at:
                    continue

                try:
                    completed_date = date.fromisoformat(completed_at)
                except ValueError:
                    continue

                delta_days = (today - completed_date).days
                if 0 <= delta_days <= 2:
                    recent_completed_ids.add(uid)

            # 处理待办任务（跳过已在本地缓存中标记为完成的）
            for item in pending_items:
                if not isinstance(item, dict):
                    continue

                uid = str(item.get("uid") or "")
                if not uid:
                    continue

                # 近期已完成：优先走本地已完成缓存，避免“待办残留”的窗口期
                if uid in recent_completed_ids:
                    continue

                status = str(item.get("status") or "needs_action")
                if status == "completed":
                    continue  # 跳过已完成（理论上不会有）

                if max_pending > 0 and pending_count >= max_pending:
                    continue
                pending_count += 1

                summary = str(item.get("summary") or "")
                spent_today_sec = int(today_tasks.get(uid, 0))

                tasks.append(
                    {
                        "id": uid,
                        "name": summary,
                        "display_name": summary,
                        "status": "needs_action",
                        "duration": default_duration,
                        "spent_today_sec": spent_today_sec,
                    }
                )

            # 从本地缓存获取已完成任务（TickTick API 不返回已完成）
            # Get completed tasks from local cache (TickTick API doesn't return completed)
            for item in cached_completed:
                if max_completed > 0 and completed_count >= max_completed:
                    break

                uid = str(item.get("id") or "")
                if not uid:
                    continue

                # 若该任务仍在待办列表里（可能是重新打开或缓存过期），则不在“已完成”中重复显示
                if any(t.get("id") == uid for t in tasks):
                    continue

                completed_count += 1
                summary = str(item.get("name") or "")
                completed_spent_sec = int(item.get("completed_spent_sec") or 0)
                spent_today_sec = completed_spent_sec or int(today_tasks.get(uid, 0))

                completed_at_mmdd = str(item.get("completed_at_mmdd") or "")
                if not completed_at_mmdd:
                    completed_at = str(item.get("completed_at") or "")
                    # 兼容旧缓存：completed_at 为 YYYY-MM-DD 时，转换成 MM.DD
                    if len(completed_at) >= 10 and completed_at[4] == "-" and completed_at[7] == "-":
                        completed_at_mmdd = f"{completed_at[5:7]}.{completed_at[8:10]}"

                tasks.append(
                    {
                        "id": uid,
                        "name": summary,
                        "display_name": summary,
                        "status": "completed",
                        "duration": default_duration,
                        "spent_today_sec": spent_today_sec,
                        "completed_at": completed_at_mmdd,
                        "completed_spent_sec": completed_spent_sec,
                    }
                )

            _LOGGER.debug("准备推送任务：待办=%d 已完成=%d", pending_count, completed_count)

            payload = {"tasks": tasks}

            try:
                async with async_timeout.timeout(5):
                    resp = await session.post(url_tasklist, json=payload)
                    resp.raise_for_status()
                _LOGGER.info("已推送任务到设备：待办=%s 已完成=%s", pending_count, completed_count)
            except (asyncio.TimeoutError, ClientError) as err:
                _LOGGER.warning("推送任务到设备失败：%s", err)
            except Exception as err:  # noqa: BLE001
                _LOGGER.exception("推送任务到设备异常：%s", err)

    async def _async_complete_and_push(task_id: str, task_name: str) -> None:
        """回写 TickTick 完成后，保存到本地缓存并推送到设备。"""

        try:
            # 注意：ticktick.complete_task 不支持 return_response，直接调用
            await hass.services.async_call(
                "ticktick",
                "complete_task",
                service_data={"projectId": ticktick_project_id, "taskId": task_id},
                blocking=True,
            )
            _LOGGER.info("TickTick 已标记完成：%s %s", task_id, task_name)

            # 保存到本地已完成缓存（因为 TickTick API 不返回已完成任务）
            # Save to local completed cache (TickTick API doesn't return completed tasks)
            await stats_store.async_mark_task_completed(
                task_id=task_id,
                task_name=task_name,
                max_completed=max_completed,
            )
            _LOGGER.info("已保存到本地已完成缓存：%s %s", task_id, task_name)

        except Exception as err:  # noqa: BLE001 - 外部服务异常需提示
            _LOGGER.exception("TickTick 回写失败：%s", err)
            await hass.services.async_call(
                "persistent_notification",
                "create",
                {
                    "title": "⚠️ TickTick 回写失败",
                    "message": f"任务 ID: {task_id}\n任务名称: {task_name}\n错误: {err}",
                    "notification_id": f"focus_dial_ticktick_error_{task_id}",
                },
                blocking=False,
            )
        finally:
            await _async_push_tasks()

    async def _handle_webhook(hass: HomeAssistant, webhook_id: str, request) -> Any:
        try:
            payload = await request.json()
        except Exception:  # noqa: BLE001 - webhook 输入不可信
            return web.Response(status=400, text="invalid json")

        event = payload.get("event")

        # 设备上线事件：自动推送任务列表
        if event == "device_online":
            _LOGGER.info("Focus Dial 设备上线，自动推送任务列表")
            hass.async_create_task(_async_push_tasks())

        elif event == "focus_completed" and bool(payload.get("count_time")):
            task_id = str(payload.get("task_id") or "")
            task_name = str(payload.get("task_name") or "")
            seconds = int(payload.get("elapsed_seconds") or 0)
            await stats_store.async_add_seconds(task_id=task_id, task_name=task_name, seconds=seconds)
        elif event == "task_done_decision":
            task_id = str(payload.get("task_id") or "")
            task_name = str(payload.get("task_name") or "")
            mark_task_done = bool(payload.get("mark_task_done"))

            # 记录专注时长（无论是否标记完成）
            elapsed_seconds = int(payload.get("elapsed_seconds") or 0)
            count_time = bool(payload.get("count_time"))
            if count_time and elapsed_seconds > 0:
                await stats_store.async_add_seconds(task_id=task_id, task_name=task_name, seconds=elapsed_seconds)
                _LOGGER.info("已记录专注时长：%s %s %d秒", task_id, task_name, elapsed_seconds)

            # YES：先回写 TickTick，再刷新并推送（任务立刻从待办消失/进入已完成）
            if mark_task_done and task_id:
                hass.async_create_task(_async_complete_and_push(task_id=task_id, task_name=task_name))
            else:
                # NO：也刷新一次，确保设备端任务列表/今日用时同步
                hass.async_create_task(_async_push_tasks())

        return web.json_response({"status": "ok"})

    webhook.async_register(hass, DOMAIN, name, webhook_id, _handle_webhook)
    _LOGGER.info("Focus Dial webhook 已注册：/api/webhook/%s", webhook_id)

    async def _handle_push_service(call: ServiceCall) -> None:
        await _async_push_tasks()

    hass.services.async_register(DOMAIN, SERVICE_PUSH_TASKS, _handle_push_service)

    return True

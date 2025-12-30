"""Focus Dial（专注拨盘）HA 自定义组件：

1) 专注统计写入 `.storage`（长期累加“总学习时长”）
2) 设备端可切换 TickTick 项目（清单），HA 推送对应项目任务列表
3) 设备端支持标记完成与子任务勾选（写回 TickTick）
"""

from __future__ import annotations

import asyncio
import logging
from datetime import datetime
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
                # 旧配置兼容：早期通过 todo 实体拉取（字段不全），现已切换为 ticktick openapi 服务
                vol.Optional(CONF_TODO_ENTITY_ID): cv.entity_id,
                # 默认项目（HA 重启或首次启动时使用；设备可切换）
                vol.Optional(CONF_TICKTICK_PROJECT_ID): cv.string,
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

def _unwrap_service_response(resp: Any) -> Any:
    """兼容不同服务实现：有的会把 payload 包一层 dict。"""
    if isinstance(resp, dict) and len(resp) == 1:
        only_val = next(iter(resp.values()))
        if isinstance(only_val, (dict, list)):
            return only_val
    return resp


def _parse_ticktick_datetime(value: str | None) -> datetime | None:
    if not value:
        return None
    text = str(value)
    for fmt in ("%Y-%m-%dT%H:%M:%S%z", "%Y-%m-%dT%H:%M:%S.%f%z"):
        try:
            return datetime.strptime(text, fmt)
        except ValueError:
            continue
    return None


def _to_mmdd(value: str | None) -> str:
    dt = _parse_ticktick_datetime(value)
    if dt is None:
        return ""
    return dt_util.as_local(dt).strftime("%m.%d")


def _priority_flag(priority: int) -> str:
    # TickTick：None 0 / Low 1 / Medium 3 / High 5
    if priority >= 5:
        return "H"
    if priority >= 3:
        return "M"
    if priority >= 1:
        return "L"
    return "-"


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


async def async_setup(hass: HomeAssistant, config: dict[str, Any]) -> bool:
    """通过 YAML 配置初始化集成。"""

    if DOMAIN not in config:
        return True

    domain_cfg = config[DOMAIN]
    name: str = domain_cfg.get(CONF_NAME, "Focus Dial")
    webhook_id: str = domain_cfg[CONF_WEBHOOK_ID]
    device_host: str = domain_cfg[CONF_DEVICE_HOST]
    todo_entity_id: str | None = domain_cfg.get(CONF_TODO_ENTITY_ID)
    default_project_id: str | None = domain_cfg.get(CONF_TICKTICK_PROJECT_ID)
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

    async def _async_ticktick_get_projects() -> list[dict[str, Any]]:
        resp = await _async_call_service_return_response(hass, "ticktick", "get_projects")
        data = _unwrap_service_response(resp)

        if isinstance(data, list):
            return [p for p in data if isinstance(p, dict)]

        if isinstance(data, dict):
            candidates = data.get("projects") or data.get("data") or data.get("result") or data.get("items")
            if isinstance(candidates, list):
                return [p for p in candidates if isinstance(p, dict)]

        return []

    async def _async_ticktick_get_project_data(project_id: str) -> dict[str, Any] | None:
        if not project_id:
            return None
        resp = await _async_call_service_return_response(
            hass, "ticktick", "get_detailed_project", service_data={"projectId": project_id}
        )
        data = _unwrap_service_response(resp)
        if isinstance(data, dict) and ("tasks" in data or "project" in data):
            return data
        return None

    def _normalize_project_list(projects: list[dict[str, Any]]) -> list[dict[str, Any]]:
        out: list[dict[str, Any]] = []
        for p in projects:
            pid = str(p.get("id") or "")
            pname = str(p.get("name") or "")
            if not pid or not pname:
                continue
            if bool(p.get("closed")):
                continue
            kind = str(p.get("kind") or "TASK")
            if kind and kind != "TASK":
                continue
            out.append({"id": pid, "name": pname, "sortOrder": int(p.get("sortOrder") or 0)})
        out.sort(key=lambda x: x.get("sortOrder", 0))
        return out

    async def _async_push_tasks(project_id: str | None = None) -> None:
        """拉取 TickTick 项目/任务并推送到设备（包含待办+已完成+子任务字段）。"""

        async with push_lock:
            stats_store.stats.ensure_today()
            today_tasks = stats_store.stats.today_tasks

            # 1) 获取项目列表
            raw_projects = await _async_ticktick_get_projects()
            projects = _normalize_project_list(raw_projects)
            if not projects:
                _LOGGER.error("未获取到 TickTick 项目列表，请检查 TickTick 集成与 OAuth 是否正常")
                # 兜底：如果用户还配置了 todo 实体，提示降级（不在本任务范围内实现降级逻辑）
                if todo_entity_id:
                    _LOGGER.warning("已配置 todo 实体 %s，但多项目/子任务需要 ticktick.get_* 服务返回数据", todo_entity_id)
                return

            # 2) 选择当前项目
            selected_project_id = (
                str(project_id or "")
                or stats_store.get_selected_project_id()
                or str(default_project_id or "")
            )
            if not selected_project_id:
                selected_project_id = projects[0]["id"]

            selected_project_name = ""
            for p in projects:
                if p["id"] == selected_project_id:
                    selected_project_name = p["name"]
                    break
            if not selected_project_name:
                # 若已保存的 project 不在列表中（被删除/关闭），回退到第一个
                selected_project_id = projects[0]["id"]
                selected_project_name = projects[0]["name"]

            await stats_store.async_set_selected_project_id(selected_project_id)

            # 3) 获取项目数据（含 tasks/items）
            project_data = await _async_ticktick_get_project_data(selected_project_id)
            if not project_data:
                _LOGGER.error("获取 TickTick 项目数据失败：%s", selected_project_id)
                return

            raw_tasks = project_data.get("tasks") or []
            if not isinstance(raw_tasks, list):
                raw_tasks = []

            pending: list[tuple[int, dict[str, Any]]] = []
            completed: list[tuple[datetime, dict[str, Any]]] = []

            for t in raw_tasks:
                if not isinstance(t, dict):
                    continue

                task_id = str(t.get("id") or "")
                if not task_id:
                    continue

                title = str(t.get("title") or "")
                if not title:
                    title = str(t.get("content") or "") or "未命名任务"

                status_num = int(t.get("status") or 0)
                status = "completed" if status_num == 2 else "needs_action"

                due_mmdd = _to_mmdd(t.get("dueDate"))
                completed_mmdd = _to_mmdd(t.get("completedTime"))

                priority = int(t.get("priority") or 0)
                repeat_flag = str(t.get("repeatFlag") or "")
                reminders = t.get("reminders") if isinstance(t.get("reminders"), list) else []

                items_raw = t.get("items") if isinstance(t.get("items"), list) else []
                subtasks: list[dict[str, Any]] = []
                sub_done = 0
                for it in items_raw:
                    if not isinstance(it, dict):
                        continue
                    item_id = str(it.get("id") or "")
                    item_title = str(it.get("title") or "")
                    item_status = int(it.get("status") or 0)
                    if not item_id or not item_title:
                        continue
                    if item_status == 1:
                        sub_done += 1
                    subtasks.append({"id": item_id, "title": item_title, "status": item_status})

                task_payload: dict[str, Any] = {
                    "id": task_id,
                    "project_id": selected_project_id,
                    "name": title,
                    "display_name": title,
                    "status": status,
                    "duration": default_duration,
                    "spent_today_sec": int(today_tasks.get(task_id, 0)),
                    "priority": priority,
                    "priority_flag": _priority_flag(priority),
                    "due_mmdd": due_mmdd,
                    "completed_mmdd": completed_mmdd,
                    "has_repeat": bool(repeat_flag),
                    "has_reminder": bool(reminders),
                    "subtasks_total": len(subtasks),
                    "subtasks_done": sub_done,
                    "subtasks": subtasks,
                }

                if status == "completed":
                    completed_dt = _parse_ticktick_datetime(t.get("completedTime")) or datetime.fromtimestamp(0, tz=dt_util.UTC)
                    completed.append((completed_dt, task_payload))
                else:
                    sort_order = int(t.get("sortOrder") or 0)
                    pending.append((sort_order, task_payload))

            pending.sort(key=lambda x: x[0])
            completed.sort(key=lambda x: x[0], reverse=True)

            pending_payload = [p for _, p in pending[: max_pending or None]]
            completed_payload = [p for _, p in completed[: max_completed or None]]

            tasks_payload = pending_payload + completed_payload

            payload = {
                "selected_project_id": selected_project_id,
                "selected_project_name": selected_project_name,
                "projects": [{"id": p["id"], "name": p["name"]} for p in projects],
                "tasks": tasks_payload,
            }

            try:
                async with async_timeout.timeout(5):
                    resp = await session.post(url_tasklist, json=payload)
                    resp.raise_for_status()
                _LOGGER.info(
                    "已推送任务到设备：project=%s 待办=%s 已完成=%s",
                    selected_project_name or selected_project_id,
                    len(pending_payload),
                    len(completed_payload),
                )
            except (asyncio.TimeoutError, ClientError) as err:
                _LOGGER.warning("推送任务到设备失败：%s", err)
            except Exception as err:  # noqa: BLE001
                _LOGGER.exception("推送任务到设备异常：%s", err)

    async def _async_complete_and_push(project_id: str, task_id: str, task_name: str) -> None:
        """回写 TickTick 完成后，刷新并推送到设备。"""

        try:
            # 注意：ticktick.complete_task 不支持 return_response，直接调用
            await hass.services.async_call(
                "ticktick",
                "complete_task",
                service_data={"projectId": project_id, "taskId": task_id},
                blocking=True,
            )
            _LOGGER.info("TickTick 已标记完成：%s %s", task_id, task_name)

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
            await _async_push_tasks(project_id=project_id)

    async def _async_toggle_subtask_and_push(project_id: str, task_id: str, item_id: str, completed: bool) -> None:
        """勾选/取消勾选子任务，并刷新推送。"""

        try:
            project_data = await _async_ticktick_get_project_data(project_id)
            if not project_data:
                raise ValueError(f"无法获取项目数据：{project_id}")

            tasks_raw = project_data.get("tasks") or []
            if not isinstance(tasks_raw, list):
                tasks_raw = []

            task_obj: dict[str, Any] | None = None
            for t in tasks_raw:
                if isinstance(t, dict) and str(t.get("id") or "") == task_id:
                    task_obj = t
                    break
            if not task_obj:
                raise ValueError(f"任务不存在：{task_id}")

            items_raw = task_obj.get("items") if isinstance(task_obj.get("items"), list) else []
            if not items_raw:
                raise ValueError("该任务没有子任务（items 为空）")

            updated_items: list[dict[str, Any]] = []
            found = False
            for it in items_raw:
                if not isinstance(it, dict):
                    continue
                if str(it.get("id") or "") != item_id:
                    updated_items.append(it)
                    continue
                found = True
                it = dict(it)
                it["status"] = 1 if completed else 0
                if completed:
                    it["completedTime"] = dt_util.utcnow().strftime("%Y-%m-%dT%H:%M:%S%z")
                else:
                    it.pop("completedTime", None)
                updated_items.append(it)

            if not found:
                raise ValueError(f"子任务不存在：{item_id}")

            await hass.services.async_call(
                "ticktick",
                "update_task",
                service_data={
                    "projectId": project_id,
                    "taskId": task_id,
                    "items": updated_items,
                },
                blocking=True,
            )
            _LOGGER.info("子任务已更新：task=%s item=%s completed=%s", task_id, item_id, completed)

        except Exception as err:  # noqa: BLE001
            _LOGGER.exception("子任务更新失败：%s", err)
            await hass.services.async_call(
                "persistent_notification",
                "create",
                {
                    "title": "⚠️ TickTick 子任务更新失败",
                    "message": f"project: {project_id}\ntask: {task_id}\nitem: {item_id}\ncompleted: {completed}\n错误: {err}",
                    "notification_id": f"focus_dial_ticktick_subtask_error_{task_id}_{item_id}",
                },
                blocking=False,
            )
        finally:
            await _async_push_tasks(project_id=project_id)

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

        elif event == "project_selected":
            project_id = str(payload.get("project_id") or "")
            if not project_id:
                return web.json_response({"status": "error", "message": "missing project_id"}, status=400)
            _LOGGER.info("设备选择项目：%s", project_id)
            await stats_store.async_set_selected_project_id(project_id)
            hass.async_create_task(_async_push_tasks(project_id=project_id))

        elif event == "focus_completed" and bool(payload.get("count_time")):
            task_id = str(payload.get("task_id") or "")
            task_name = str(payload.get("task_name") or "")
            seconds = int(payload.get("elapsed_seconds") or 0)
            await stats_store.async_add_seconds(task_id=task_id, task_name=task_name, seconds=seconds)
        elif event == "task_done_decision":
            project_id = str(payload.get("project_id") or "") or stats_store.get_selected_project_id() or str(default_project_id or "")
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
            if mark_task_done and task_id and project_id:
                hass.async_create_task(_async_complete_and_push(project_id=project_id, task_id=task_id, task_name=task_name))
            else:
                # NO：也刷新一次，确保设备端任务列表/今日用时同步
                hass.async_create_task(_async_push_tasks())

        elif event == "task_complete":
            project_id = str(payload.get("project_id") or "") or stats_store.get_selected_project_id() or str(default_project_id or "")
            task_id = str(payload.get("task_id") or "")
            task_name = str(payload.get("task_name") or "")
            if not project_id or not task_id:
                return web.json_response({"status": "error", "message": "missing project_id/task_id"}, status=400)
            hass.async_create_task(_async_complete_and_push(project_id=project_id, task_id=task_id, task_name=task_name))

        elif event == "subtask_toggle":
            project_id = str(payload.get("project_id") or "") or stats_store.get_selected_project_id() or str(default_project_id or "")
            task_id = str(payload.get("task_id") or "")
            item_id = str(payload.get("item_id") or "")
            completed = bool(payload.get("completed"))
            if not project_id or not task_id or not item_id:
                return web.json_response({"status": "error", "message": "missing project_id/task_id/item_id"}, status=400)
            hass.async_create_task(
                _async_toggle_subtask_and_push(project_id=project_id, task_id=task_id, item_id=item_id, completed=completed)
            )

        return web.json_response({"status": "ok"})

    webhook.async_register(hass, DOMAIN, name, webhook_id, _handle_webhook)
    _LOGGER.info("Focus Dial webhook 已注册：/api/webhook/%s", webhook_id)

    async def _handle_push_service(call: ServiceCall) -> None:
        await _async_push_tasks(project_id=call.data.get("project_id"))

    hass.services.async_register(DOMAIN, SERVICE_PUSH_TASKS, _handle_push_service)

    return True

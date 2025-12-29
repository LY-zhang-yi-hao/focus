"""将专注统计写入 Home Assistant 的 .storage（适合长期累加总学习时长）。"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Any

from homeassistant.core import HomeAssistant
from homeassistant.helpers.storage import Store
from homeassistant.util import dt as dt_util

from .const import STORAGE_KEY, STORAGE_VERSION


def _today_str() -> str:
    return dt_util.now().date().isoformat()


def _today_mmdd_str() -> str:
    """返回用于设备显示的完成日期（MM.DD）。"""
    return dt_util.now().strftime("%m.%d")


@dataclass
class FocusDialStats:
    """内存中的统计结构。"""

    today_date: str
    today_total_seconds: int
    today_tasks: dict[str, int]
    total_seconds: int
    tasks_total: dict[str, dict[str, Any]]
    completed_tasks: list[dict[str, Any]]  # 已完成任务缓存（最近 N 个）

    @classmethod
    def default(cls) -> "FocusDialStats":
        return cls(
            today_date=_today_str(),
            today_total_seconds=0,
            today_tasks={},
            total_seconds=0,
            tasks_total={},
            completed_tasks=[],
        )

    @classmethod
    def from_dict(cls, data: dict[str, Any] | None) -> "FocusDialStats":
        if not isinstance(data, dict):
            return cls.default()

        return cls(
            today_date=str(data.get("today_date") or _today_str()),
            today_total_seconds=int(data.get("today_total_seconds") or 0),
            today_tasks=dict(data.get("today_tasks") or {}),
            total_seconds=int(data.get("total_seconds") or 0),
            tasks_total=dict(data.get("tasks_total") or {}),
            completed_tasks=list(data.get("completed_tasks") or []),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "today_date": self.today_date,
            "today_total_seconds": self.today_total_seconds,
            "today_tasks": self.today_tasks,
            "total_seconds": self.total_seconds,
            "tasks_total": self.tasks_total,
            "completed_tasks": self.completed_tasks,
        }

    def ensure_today(self) -> None:
        today = _today_str()
        if self.today_date == today:
            return
        self.today_date = today
        self.today_total_seconds = 0
        self.today_tasks = {}

    def add_seconds(self, task_id: str, task_name: str, seconds: int) -> None:
        if seconds <= 0:
            return

        self.ensure_today()

        self.total_seconds += seconds
        self.today_total_seconds += seconds

        if task_id:
            self.today_tasks[task_id] = int(self.today_tasks.get(task_id, 0)) + seconds

            task_total = self.tasks_total.get(task_id) or {}
            task_total["seconds"] = int(task_total.get("seconds", 0)) + seconds
            if task_name:
                task_total["name"] = task_name
            self.tasks_total[task_id] = task_total

    def mark_task_completed(
        self,
        task_id: str,
        task_name: str,
        max_completed: int = 10,
        completed_spent_sec: int | None = None,
        completed_at_mmdd: str | None = None,
    ) -> None:
        """将任务添加到已完成缓存（去重，保留最近 N 个）。"""
        if not task_id:
            return

        # 移除已存在的同 ID 任务（避免重复）
        self.completed_tasks = [t for t in self.completed_tasks if t.get("id") != task_id]

        # 添加到列表开头
        self.completed_tasks.insert(
            0,
            {
                "id": task_id,
                "name": task_name,
                "display_name": task_name,
                "status": "completed",
                # 用于持久化与排查：ISO 日期（YYYY-MM-DD）
                "completed_at": _today_str(),
                # 用于设备显示：MM.DD
                "completed_at_mmdd": completed_at_mmdd or _today_mmdd_str(),
                # 完成当天该任务累计专注秒数（用于设备显示“专注xxmin”）
                "completed_spent_sec": int(completed_spent_sec or 0),
            },
        )

        # 保留最近 N 个
        if len(self.completed_tasks) > max_completed:
            self.completed_tasks = self.completed_tasks[:max_completed]

    def get_completed_tasks(self) -> list[dict[str, Any]]:
        """获取已完成任务缓存。"""
        return self.completed_tasks


class FocusDialStatsStore:
    """带锁的 .storage 读写封装。"""

    def __init__(self, hass: HomeAssistant) -> None:
        self._hass = hass
        self._store: Store[dict[str, Any]] = Store(hass, STORAGE_VERSION, STORAGE_KEY)
        self._lock = asyncio.Lock()
        self._stats = FocusDialStats.default()

    @property
    def stats(self) -> FocusDialStats:
        return self._stats

    async def async_load(self) -> None:
        async with self._lock:
            raw = await self._store.async_load()
            self._stats = FocusDialStats.from_dict(raw)
            self._stats.ensure_today()

    async def async_save(self) -> None:
        async with self._lock:
            await self._store.async_save(self._stats.to_dict())

    async def async_add_seconds(self, task_id: str, task_name: str, seconds: int) -> None:
        async with self._lock:
            self._stats.add_seconds(task_id=task_id, task_name=task_name, seconds=seconds)
            await self._store.async_save(self._stats.to_dict())

    async def async_mark_task_completed(self, task_id: str, task_name: str, max_completed: int = 10) -> None:
        """将任务标记为已完成并保存到缓存。"""
        async with self._lock:
            self._stats.ensure_today()
            completed_spent_sec = int(self._stats.today_tasks.get(task_id, 0))
            self._stats.mark_task_completed(
                task_id=task_id,
                task_name=task_name,
                max_completed=max_completed,
                completed_spent_sec=completed_spent_sec,
            )
            await self._store.async_save(self._stats.to_dict())

    def get_completed_tasks(self) -> list[dict[str, Any]]:
        """获取已完成任务缓存。"""
        return self._stats.get_completed_tasks()

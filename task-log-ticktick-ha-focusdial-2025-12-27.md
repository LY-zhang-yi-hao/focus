# 任务日志 - TickTick（#focus）× Home Assistant × Focus Dial 重构（2025-12-27）

## 需求与背景

- 目标：在 Focus Dial（硬件番茄钟）上接收并选择 TickTick 的 `#focus` 任务，开始计时，并统计“今天每个任务累计专注用时”。
- 架构：采用方案 B（Home Assistant 作为中间层），由 HA 负责获取 TickTick 任务与统计，Focus Dial 负责展示/选择/计时与事件上报。
- 统计口径：
  - 仅“番茄钟正常结束（计时到 0）”的时长计入“今日累计用时”。
  - 暂停/取消均不计入“今日累计用时”。
- 回写要求：
  - 每次番茄钟结束或中途取消时，在设备上给出选项：是否将 TickTick 任务标记为“已完成”。
  - 由 HA 根据设备上报的选择去执行 TickTick 回写。

## 计划（改动前）

1. 固件数据模型整理：统一 `FocusTask` 结构，补充 `spent_today_sec`（今日累计秒数）字段。
2. 修复编译/接口问题：修正 `DisplayController` 与 `TaskListState` 的任务类型不一致，并重构 webhook 发送为“发送完整 JSON payload”（避免二次包裹导致 JSON 非法）。
3. 任务选择与会话管理：
   - 选择任务后，将 task_id/task_name 写入当前会话上下文。
   - 会话开始/暂停/恢复/结束/取消分别上报给 HA。
4. 结束/取消后的“是否完成任务”交互：
   - 新增一个结束确认状态：旋钮选择 YES/NO，按键确认；默认：结束=YES、取消=NO。
5. HA 侧对接说明：
   - 约定下发任务列表 `/api/tasklist` 的 payload（含 `spent_today_sec`）。
   - 约定设备上报 webhook 的事件字段（含 `mark_task_done`、`count_time`）。
   - 提供示例 YAML：今日统计的存储/清零、推送任务列表、接收事件并自动回写 TickTick 的脚手架。

## 预期效果

- Focus Dial 可显示任务列表（来自 HA），每个任务可展示“今日累计用时”提示，并可旋钮选择开始计时。
- 计时完成/取消后，设备提示“是否标记任务完成”，用户可选择后确认。
- 设备向 HA 上报结构化 JSON：
  - `action=start/stop`（兼容旧自动化）
  - `reason=timer_done/canceled/...`
  - `task_id/task_name/session_id/elapsed_seconds/count_time/mark_task_done`
- HA：
  - 仅在 `count_time=true` 时累加 `elapsed_seconds` 到“今日该任务累计用时”
  - 在 `mark_task_done=true` 时自动回写 TickTick（具体通过 HA 的 TickTick 集成或 REST 调用实现）

## 实际结果（改动后补充）

- 已新增 FocusTask 数据模型（含 `spentTodaySeconds`），用于承载 HA 下发的任务列表与“今日累计用时”。
- 已新增 `TaskListState`：
  - 支持在设备上滚动选择任务、显示 `Dxxm / Txxm(Thhmm)` 信息，并按键开始计时。
  - 空闲界面支持双击进入任务列表（若未下发任务会提示 `NO TASKS`）。
- 已新增 `TaskCompletePromptState`：
  - 计时正常结束或用户取消后（仅限有 task_id 的任务），弹出 YES/NO 选择“是否标记 TickTick 完成”。
  - 选择结果通过 webhook 以 `event=task_done_decision` 上报给 HA。
- 已重构 webhook 发送机制：
  - `NetworkController` 增加 `sendWebhookPayload()`，支持发送完整 JSON payload（避免旧实现二次包裹导致 JSON 不合法的问题）。
  - 计时全流程上报：`focus_started/focus_resumed/focus_paused/focus_canceled/focus_completed`，并保留 `action=start/stop` 兼容旧自动化。
- 已在设备端新增 HTTP API（供 HA 下发任务列表）：
  - `POST /api/tasklist`：接收任务列表 JSON，并触发回调更新任务列表。
  - `GET /api/status`：返回基础状态（WiFi/是否已加载任务列表）。
- 已补充 HA 联动文档与自测命令：`HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md`。

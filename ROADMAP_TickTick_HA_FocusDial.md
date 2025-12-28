# 后续任务规划（Roadmap）：Focus Dial × TickTick（#focus）× Home Assistant（HA）

更新时间：2025-12-28  
架构选择：方案 B（HA 中间层）  
设备信息：Focus Dial `192.168.15.166`（MAC `64:B7:08:84:12:D4`）

---

## M0（已完成）现状基线

- 设备 HTTP API：
  - `GET http://192.168.15.166/api/status`
  - `POST http://192.168.15.166/api/tasklist`
- 设备交互：
  - 空闲界面双击进入 `SELECT TASK`
  - 旋钮选择任务，按键开始计时
  - 结束/取消后弹窗 YES/NO：是否标记任务完成，并上报 `task_done_decision`
- 统计口径（已固化）：
  - 仅 `focus_completed` 且 `count_time=true` 计入今日累计
  - `pause/cancel` 不计入
- 中文显示（已修复 A）：
  - 支持 `display_name`（设备端优先显示），解决中文标题在小字库下显示为空白的问题

---

## M1（P0）HA 推送任务列表闭环（含 display_name）

### 目标

- Home Assistant 能稳定把 TickTick 的 `#focus` 任务列表推送到设备 `192.168.15.166`，并确保设备端“看得到、选得了”。

### 范围

- TickTick → HA：拉取任务并过滤 `#focus`
- HA → 设备：周期性 + 手动触发推送 `/api/tasklist`
- 生成 `display_name`：确保设备端可显示（ASCII/拼音/简称/编号）

### 交付物

- `rest_command.focus_dial_push_tasklist`（指向 `http://192.168.15.166/api/tasklist`）
- `script.focus_dial_send_ticktick_focus_tasks`（组装 payload：`id/name/display_name/duration/spent_today_sec`）
- 一个手动触发入口：
  - HA 面板按钮/脚本按钮（“立即推送任务”）

### 验收标准

- 在 HA 执行“立即推送任务”后：
  - `curl http://192.168.15.166/api/status` 显示 `tasklist_loaded:true`
  - 设备 `SELECT TASK` 能显示至少 1 条任务，并可选择启动计时

### 风险与回退

- 风险：HA 生成的 JSON 不合法（逗号/转义/数组为空）
  - 回退：先用 `curl` 固定 payload 验证设备端，再对齐 HA 模板

---

## M2（P0）今日累计统计（设备显示 + HA 面板显示）

### 目标

- 每个任务的“今日累计专注时长”能正确累加并展示：
  - 设备任务列表底部显示 `Txxm / Txxhmm`
  - HA 面板显示每任务今日累计

### 范围

- HA webhook 接收 `focus_completed`：
  - 满足：`event == focus_completed && count_time == true && task_id != ''`
  - 动作：把 `elapsed_seconds` 累加到今日统计（按 task_id）
- 每天 0 点清空今日统计
- 推送任务列表时将 `spent_today_sec` 写入每条任务

### 交付物

- `input_text.focus_dial_stats_today`（JSON 字典，key=task_id, value=seconds）
- 累加实现（推荐优先）：
  - 自动化内 Jinja：`combine + to_json`（零依赖，已验证可用）
  - （可选）`pyscript` / Node-RED flow（适合更复杂的存储与去重）
- Dashboard 卡片：展示今日累计（按任务）

### 验收标准

- 正常完成一次计时（`focus_completed`）后：
  - HA 的统计实体对应任务 seconds 增长
  - 重新推送 tasklist 后设备端显示的 `Txxm` 随之变化
- 取消/暂停不会导致统计增长
- 每天 0 点统计清空

### 风险与回退

- 风险：重复 webhook（网络重试）导致重复累计
  - 回退：M5 引入 `session_id` 去重；M2 阶段先靠短期观测确认 webhook 是否会重复

---

## M3（P0）自动回写 TickTick（用户确认“已完成”）

### 目标

- 当设备弹窗选择 YES（`task_done_decision.mark_task_done == true`）时，HA 自动把该任务回写为 TickTick “已完成”。

### 推荐实现路径（你确认：优先用现有 HA/TickTick 集成服务）

1) 先确认你当前 HA 中 TickTick 集成是否提供“完成任务”的 service（或类似 action）。  
2) 在 webhook 自动化中，对 `task_done_decision` 做 choose：
   - `mark_task_done == true` → 调用 TickTick 集成服务完成该 `task_id`
   - `mark_task_done == false` → 不回写

### 交付物

- webhook automation：处理 `task_done_decision`
- TickTick 完成任务的 service 调用（以你现有集成能力为准）
- 失败告警：
  - service 调用失败 → persistent_notification + 记录 payload（便于重放/补偿）

### 验收标准

- 设备上选择 YES 后：
  - TickTick 里对应任务变为“已完成”
- 选择 NO 不会影响 TickTick 任务状态
- 回写失败可被发现（有告警）且可手动补偿

### 风险与回退

- 风险：现有 TickTick 集成不提供“完成任务”的服务
  - 回退方案 A：Node-RED 调用 API（作为备选，不影响 M1/M2 进度）
  - 回退方案 B：本地小服务（HA 调用 REST，服务再调 TickTick API）

---

## M4（P1）HA 面板与可观测性（运维友好）

### 目标

- 不看日志也能定位“为什么设备没任务/为什么不累计/为什么不回写”。

### 交付物

- Dashboard（建议 3 块区域）：
  - 设备在线状态：WiFi、`tasklist_loaded`、最后推送时间
  - 今日统计：按任务列表展示 `spent_today`
  - 最近一次会话：最近 webhook payload（只保留必要字段）
- 调试工具：
  - “立即推送任务”按钮
  - “清空今日统计”按钮（仅调试用，生产可隐藏）

### 验收标准

- 面板能一眼看出：
  - 设备是否在线/是否收到 tasklist
  - 今日统计是否在增长
  - 最近一次 webhook 的 event/任务 id/时长

---

## M5（P1）可靠性与边界（去重/重试/断网）

### 目标

- 让系统在网络波动、设备重启、HA 重启时仍可稳定运行，避免“重复累计/重复回写”。

### 交付物

- 去重：
  - 按 `session_id + event` 做幂等处理（记录已处理集合，设置 TTL）
- 重试与告警：
  - 推送 tasklist 失败重试（指数退避或固定次数）
  - TickTick 回写失败重试 + 告警 + 手动补偿入口
- 异常策略：
  - 设备离线：HA 记录待推送，设备恢复后补推

### 验收标准

- 重复 webhook 不会导致重复累计
- HA 重启后统计仍准确（今日统计不丢）
- 推送/回写失败时可被发现并可恢复

---

## M6（P2）体验增强（可选）

### 目标

- 在不增加复杂度的前提下，提升设备端与日常使用体验。

### 候选项（按投入产出排序）

1) 设备端显示更多调试信息（可选开关）：
   - IP、任务数、最后更新时间
2) 任务排序策略：
   - 按优先级/到期时间/自定义顺序
3) 若强需求原生中文显示：
   - 评估中文字体引入/渲染方案（体积、性能、开发成本）

### 验收标准

- 增强项不破坏既有链路，且对日常使用有明显收益。

---

## 立即行动清单（建议你现在做的三件事）

1) 在 HA 里做 M1：把 TickTick `#focus` 任务推送到 `192.168.15.166`，并生成 `display_name`。  
2) 做 M2：今日统计累加 + 每日清零，并把统计回填到 `spent_today_sec`。  
3) 做 M3：确认 TickTick 集成是否有“完成任务”的 service；有则直接接入回写，没有就启用回退方案。

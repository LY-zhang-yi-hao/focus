# Focus Dial × TickTick（#focus）× Home Assistant（HA）联动方案

> 目标：HA 负责“取 TickTick 任务 + 今日累计用时统计 + 回写 TickTick”，Focus Dial 负责“显示/选择任务 + 计时 + 上报事件 + 结束/取消后确认是否完成任务”。

---

## 1. Focus Dial 固件能力（本仓库实现）

### 1.1 HA 下发任务列表（HTTP → Focus Dial）

Focus Dial 在局域网内提供 HTTP API（端口 80）：

- `POST http://FOCUS_DIAL_IP/api/tasklist`

请求体（示例）：

```json
{
  "tasks": [
    {
      "id": "ticktick:abcd1234",
      "name": "🍅学习 Rust",
      "display_name": "🍅学习 Rust",
      "status": "needs_action",
      "duration": 25,
      "spent_today_sec": 1800
    },
    {
      "id": "ticktick:efgh5678",
      "name": "写周报",
      "display_name": "写周报",
      "status": "completed",
      "duration": 25,
      "spent_today_sec": 1800,
      "completed_at": "12.29",
      "completed_spent_sec": 1800
    }
  ]
}
```

字段说明：

- `id`：任务唯一 ID（建议用 TickTick 的 task_id；也可前缀化如 `ticktick:...`）。
- `name`：任务名称。
- `display_name`：设备显示名（可选，兼容字段）。当前固件已支持中文字体，**无需拼音/ASCII**；通常可直接与 `name` 相同。
- `status`：任务状态：`needs_action`（待办）/ `completed`（已完成）。设备端可双击切换查看两类列表。
- `duration`：本次建议专注时长（分钟），设备选择任务后会用它启动计时。
- `spent_today_sec`：今天该任务已累计专注用时（秒），用于设备端展示。
- `completed_at`：完成日期（`MM.DD`，可选，仅 `status=completed` 时下发），用于设备端“已完成”列表底部显示。
- `completed_spent_sec`：完成当天该任务累计专注用时（秒，可选，仅 `status=completed` 时下发），设备端会四舍五入显示为 `专注xxmin`。

### 1.2 Focus Dial 上报事件（Webhook → HA）

在配网页面配置 `Webhook URL`（建议指向 HA webhook），Focus Dial 会向该 URL `POST application/json` 上报事件。

> 兼容：`action` 字段仍保留 `start/stop`，以兼容你旧的灯光/勿扰自动化。

#### A) 开始/恢复

```json
{
  "action": "start",
  "event": "focus_started",
  "session_id": "12345-678",
  "task_id": "ticktick:abcd1234",
  "task_name": "写周报 #focus",
  "duration_minutes": 25
}
```

`event` 可能值：`focus_started` / `focus_resumed`

#### B) 暂停（不计入今日统计）

```json
{
  "action": "stop",
  "event": "focus_paused",
  "session_id": "12345-678",
  "task_id": "ticktick:abcd1234",
  "task_name": "写周报 #focus",
  "elapsed_seconds": 300,
  "count_time": false
}
```

#### C) 取消（不计入今日统计）

```json
{
  "action": "stop",
  "event": "focus_canceled",
  "session_id": "12345-678",
  "task_id": "ticktick:abcd1234",
  "task_name": "写周报 #focus",
  "elapsed_seconds": 420,
  "count_time": false
}
```

#### D) 正常结束（计入今日统计）

```json
{
  "action": "stop",
  "event": "focus_completed",
  "session_id": "12345-678",
  "task_id": "ticktick:abcd1234",
  "task_name": "写周报 #focus",
  "elapsed_seconds": 1500,
  "count_time": true
}
```

#### E) 结束/取消后的“是否标记完成”选择结果

设备端会弹窗“是/否”；用户确认后再上报：

```json
{
  "action": "focus_result",
  "event": "task_done_decision",
  "session_id": "12345-678",
  "task_id": "ticktick:abcd1234",
  "task_name": "写周报 #focus",
  "mark_task_done": true,
  "end_type": "completed"
}
```

`end_type` 可能值：`completed` / `canceled`

---

## 2. HA 侧配置（推荐：方案 2 稳 / 自定义组件）

> 目标：用一个简单的 HA 自定义组件把统计写入 `.storage`（长期累加“总学习时长”），并在设备选择“是”（回写 TickTick 完成）后，**自动刷新并重新推送任务列表到设备**（待办立刻消失/进入已完成）。

### 2.1 安装自定义组件

将本仓库目录 `custom_components/focus_dial/` 复制到 HA 配置目录：

- HA OS / Supervised：`/config/custom_components/focus_dial/`
- Core：`<你的 HA 配置目录>/custom_components/focus_dial/`

重启 HA。

### 2.2 configuration.yaml 配置

在 `configuration.yaml` 添加（示例）：

```yaml
focus_dial:
  device_host: 192.168.15.166           # Focus Dial 设备 IP
  todo_entity_id: todo.zhuan_zhu        # TickTick 的 todo 实体（🎃专注清单）
  ticktick_project_id: "69510f448f0805fa66144dc9"
  webhook_id: focus_dial                # 与设备配网页面一致
  default_duration: 25
  max_pending: 20
  max_completed: 20
```

### 2.3 使用方式（推送 / 统计 / 自动刷新）

1. 设备配网页面填写 Webhook URL：`http://<HA_IP>:8123/api/webhook/focus_dial`
2. 手动推送任务到设备：开发者工具 → 服务 → 调用 `focus_dial.push_tasks`
3. 统计持久化位置：HA 的 `.storage/focus_dial_stats`（含今日/总计/按任务累加）
4. 设备选择“是”后：
   - 组件调用 `ticktick.complete_task`
   - 立刻 `todo.get_items` 刷新列表并 `POST /api/tasklist` 推送到设备
   - 设备端任务会从“待办”消失，并可在“已完成”列表看到（双击切换）
   - 说明：TickTick 的 `todo` 实体刷新可能有延迟；组件会对“近期已完成任务”做本地过滤，确保点击 YES 后立即生效

### 2.4 旧版纯 YAML（可选）

如果你暂时不想装自定义组件，可以继续使用仓库内的示例 `ha-ticktick-focusdial-config.yaml`（但不再推荐：统计会受 `input_text` 长度限制，且“完成后自动刷新推送”需要你在自动化里自己补齐）。

---

## 3. 设备端操作建议

- **空闲界面双击按键**：进入任务列表（若 HA 推送过任务列表）。
- **任务列表界面**：
  - 旋钮选择任务；**单击**在“待办”列表中开始计时
  - **双击**切换查看“待办 / 已完成”
  - 底部显示“建议xx分 / 今日xx分（或 x:xx）”
- **计时结束 / 取消**：设备弹窗“是/否”，确认是否回写 TickTick 完成；HA 收到后执行回写并自动刷新推送。

---

## 4. 快速自测（不用 HA 也能验证固件）

### 4.1 推送任务列表

```bash
curl -X POST "http://FOCUS_DIAL_IP/api/tasklist" \
  -H "Content-Type: application/json" \
  -d '{
    "tasks":[
      {"id":"ticktick:1","name":"学习 Rust","display_name":"学习 Rust","status":"needs_action","duration":25,"spent_today_sec":1800},
      {"id":"ticktick:2","name":"写周报","display_name":"写周报","status":"needs_action","duration":50,"spent_today_sec":0},
      {"id":"ticktick:3","name":"复盘","display_name":"复盘","status":"completed","duration":30,"spent_today_sec":3660,"completed_at":"12.29","completed_spent_sec":3660}
    ]
  }'
```

预期：设备自动切到任务列表（空闲/休眠时），或你在空闲界面双击进入任务列表。

### 4.2 查看状态

```bash
curl "http://FOCUS_DIAL_IP/api/status"
```

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
      "name": "写周报 #focus",
      "display_name": "XIEZHOUBAO #focus",
      "duration": 25,
      "spent_today_sec": 1800
    }
  ]
}
```

字段说明：

- `id`：任务唯一 ID（建议用 TickTick 的 task_id；也可前缀化如 `ticktick:...`）。
- `name`：任务名称。
- `display_name`：设备显示名（可选，建议 ASCII/拼音/简称）。用于解决 OLED 英文字库下中文标题显示为空白的问题；设备端会优先显示该字段。
- `duration`：本次建议专注时长（分钟），设备选择任务后会用它启动计时。
- `spent_today_sec`：今天该任务已累计专注用时（秒），用于设备端展示。

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

设备端会弹窗 YES/NO；用户确认后再上报：

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

## 2. HA 侧配置（示例）

下面以“你已经能在 HA 拿到 TickTick 的 #focus 任务列表”为前提（例如某个 `sensor.ticktick_focus_tasks` 的 attributes 里包含数组）。

### 2.1 今日累计用时存储（input_text）

在 HA 创建一个 helper：

- `input_text.focus_dial_stats_today`
- 初始值：`{}`

每天 0 点清空：

```yaml
automation:
  - id: focus_dial_reset_stats_today
    alias: "Focus Dial - 每日清空统计"
    trigger:
      - platform: time
        at: "00:00:00"
    action:
      - service: input_text.set_value
        data:
          entity_id: input_text.focus_dial_stats_today
          value: "{}"
```

### 2.2 下发任务列表到 Focus Dial（rest_command）

```yaml
rest_command:
  focus_dial_push_tasklist:
    url: "http://FOCUS_DIAL_IP/api/tasklist"
    method: POST
    content_type: "application/json"
    payload: >
      {{ tasks_json }}
```

然后写一个脚本把 TickTick 任务 + 今日统计拼成 payload：

```yaml
script:
  focus_dial_send_ticktick_focus_tasks:
    alias: "Focus Dial - 推送 TickTick #focus 任务"
    sequence:
      - variables:
          stats: "{{ states('input_text.focus_dial_stats_today') | default('{}') | from_json }}"
          tasks_json: >
            {
              "tasks": [
                {% for t in state_attr('sensor.ticktick_focus_tasks', 'tasks') | default([]) %}
                {
                  "id": "{{ t.id }}",
                  "name": "{{ t.title }}",
                  "display_name": "{{ t.display_name | default('') }}",
                  "duration": {{ t.duration | default(25) }},
                  "spent_today_sec": {{ stats.get(t.id, 0) | int }}
                }{{ ',' if not loop.last else '' }}
                {% endfor %}
              ]
            }
      - service: rest_command.focus_dial_push_tasklist
        data:
          tasks_json: "{{ tasks_json }}"
```

> 你需要把 `sensor.ticktick_focus_tasks` 的结构对齐（比如 `t.id / t.title / t.duration`），这是示例占位。
>
> 如果你的任务标题大多是中文，建议在 HA 侧生成 `display_name`（拼音/英文简称/编号均可），否则设备端可能只显示高亮条但文字为空白。

### 2.3 接收 Focus Dial webhook 并累计统计

假设你的 HA webhook id 是 `focus_dial`：

```yaml
automation:
  - id: focus_dial_webhook_events
    alias: "Focus Dial - 接收事件并统计/回写"
    mode: queued
    trigger:
      - platform: webhook
        webhook_id: focus_dial
        allowed_methods: [POST]
    variables:
      payload: "{{ trigger.json }}"
      ticktick_project_id: "69510f448f0805fa66144dc9"
      stats_entity: "input_text.focus_dial_stats_today"
      stats: "{{ (states(stats_entity) | default('{}')) | from_json | default({}) }}"
    action:
      - choose:
          # 仅当正常结束且 count_time=true 才累计
          - conditions:
              - condition: template
                value_template: >
                  {{ payload.event == 'focus_completed'
                     and (payload.count_time | default(false))
                     and (payload.task_id | default('')) != ''
                     and (payload.elapsed_seconds | default(0) | int) > 0 }}
            sequence:
              - variables:
                  task_id: "{{ payload.task_id }}"
                  add_seconds: "{{ payload.elapsed_seconds | int }}"
                  next_total: "{{ (stats.get(task_id, 0) | int) + add_seconds }}"
                  new_stats: "{{ stats | combine({ task_id: next_total }) }}"
              - service: input_text.set_value
                data:
                  entity_id: "{{ stats_entity }}"
                  value: "{{ new_stats | to_json }}"

          # 用户选择是否标记 TickTick 完成
          - conditions:
              - condition: template
                value_template: >
                  {{ payload.event == 'task_done_decision'
                     and (payload.mark_task_done | default(false))
                     and (payload.task_id | default('')) != '' }}
            sequence:
              # 注意：payload.task_id 必须是 TickTick 任务真实 uid（来自 todo.get_items 的 item.uid）
              - service: ticktick.complete_task
                data:
                  projectId: "{{ ticktick_project_id }}"
                  taskId: "{{ payload.task_id }}"
                continue_on_error: true
                response_variable: complete_response

              - choose:
                  - conditions:
                      - condition: template
                        value_template: "{{ complete_response.error is not defined }}"
                    sequence:
                      - service: system_log.write
                        data:
                          message: "Focus Dial: Completed TickTick task {{ payload.task_id }}"
                          level: info
                default:
                  - service: persistent_notification.create
                    data:
                      title: "⚠️ TickTick 回写失败"
                      message: |
                        任务 ID: {{ payload.task_id }}
                        任务名称: {{ payload.task_name }}
                        错误: {{ complete_response.error | default('Unknown error') }}
                      notification_id: "focus_dial_ticktick_error_{{ payload.task_id }}"
        default: []
```

---

## 3. 设备端操作建议

- **空闲界面双击按键**：进入任务列表（若 HA 推送过任务列表）。
- **任务列表界面**：旋钮选择任务，按键开始计时；底部显示 `Dxxm`（建议时长）与 `Txxm / Thhmm`（今日累计）。
- **计时结束 / 取消**：设备弹窗 YES/NO，确认是否标记 TickTick 完成；HA 收到后执行回写。

---

## 4. 快速自测（不用 HA 也能验证固件）

### 4.1 推送任务列表

```bash
curl -X POST "http://FOCUS_DIAL_IP/api/tasklist" \
  -H "Content-Type: application/json" \
  -d '{
    "tasks":[
      {"id":"ticktick:1","name":"写周报 #focus","display_name":"XZB #focus","duration":25,"spent_today_sec":1800},
      {"id":"ticktick:2","name":"代码评审 #focus","display_name":"REVIEW #focus","duration":50,"spent_today_sec":0},
      {"id":"ticktick:3","name":"复盘 #focus","display_name":"FUPAN #focus","duration":30,"spent_today_sec":3660}
    ]
  }'
```

预期：设备自动切到任务列表（空闲/休眠时），或你在空闲界面双击进入任务列表。

### 4.2 查看状态

```bash
curl "http://FOCUS_DIAL_IP/api/status"
```

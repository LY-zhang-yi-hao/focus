# TickTick × Home Assistant × Focus Dial 集成项目进度报告

**生成时间**：2025-12-28 21:50
**作者**：浮浮酱 φ(≧ω≦*)♪
**项目目标**：实现 Focus Dial 通过 Home Assistant 与 TickTick 的完整集成

---

## ✅ 当前问题与本轮修复目标（`homeassistant.local` NXDOMAIN）

### 现象

- 访问 `homeassistant.local` 报错：`DNS_PROBE_FINISHED_NXDOMAIN`（手机/电脑都无法解析）。
- 但 `http://192.168.15.194:8123` 与设备 `http://192.168.15.166` 在当前网络下可达。

### 根因

- `homeassistant.local` 依赖 mDNS（.local 解析）。在你的网络/手机环境中 mDNS 未生效，所以会 NXDOMAIN。

### 解决策略（推荐）

1) **统一使用可达的 IP 地址**（本轮以 `192.168.15.194` 为准），不要依赖 `homeassistant.local`。  
2) 在 HA 中把 `homeassistant.internal_url/external_url` 设为 `http://192.168.15.194:8123`，让 OAuth/跳转链接也生成 IP。  
3) TickTick Developer 的 redirect URL 列表里保留你实际使用的回调地址（见下文“Redirect URLs”）。

## 📊 项目总体进度

```
总进度：80% ████████████████░░░░

Phase 0: TickTick OAuth 认证        ✅ 100% ████████████████████
Phase 1: M1 - 推送任务列表          ✅  90% ██████████████████░░
Phase 2: M2 - 今日累计统计          ✅ 100% ████████████████████
Phase 3: M3 - 自动回写 TickTick     ✅  80% ████████████████░░░░
```

---

## ✅ 已完成的工作

### **Phase 0: TickTick OAuth 认证**（100% 完成）

#### 1. HACS 安装 ✅
- **位置**：`/home/zyh/homeassistant/custom_components/hacs/`
- **状态**：已安装并正常运行
- **验证**：HA 启动日志显示 HACS 加载成功

#### 2. TickTick 集成安装 ✅
- **来源**：https://github.com/Hantick/ticktick-home-assistant
- **位置**：`/home/zyh/homeassistant/custom_components/ticktick/`
- **版本**：最新版（2025-12-28 克隆）
- **状态**：已安装并正常运行

#### 3. TickTick Developer 应用配置 ✅
- **Client ID**：`hTqUw97rjPwspw8J4X`
- **Client Secret**：`******（已脱敏，见 TickTick Developer 控制台）`
- **OAuth Redirect URLs**：
  ```
  https://my.home-assistant.io/redirect/oauth
  http://192.168.15.194:8123/auth/external/callback
  http://localhost:8123/auth/external/callback
  ```
- **状态**：已配置并验证通过

#### 4. Home Assistant Application Credentials 配置 ✅
- **配置文件**：`/home/zyh/homeassistant/.storage/application_credentials`
- **状态**：已成功保存凭据
- **验证**：JSON 文件显示 TickTick 凭据已注册

#### 5. OAuth 认证成功 ✅
- **认证时间**：2025-12-28 19:09:32
- **状态**：Successfully authenticated
- **已创建配置**：为 "Ticktick" 创建了配置
- **解决方案**：
  - 临时：把回跳链接中的 `homeassistant.local` 手动替换成 `192.168.15.194` 完成回调
  - 长期（推荐）：把 HA `internal_url/external_url` 固定为 `http://192.168.15.194:8123`，避免再次生成 `homeassistant.local`

#### 6. TickTick Todo 实体创建 ✅
- **实体总数**：9 个（8 个旧清单 + 1 个新清单）
- **关键实体**：`todo.zhuan_zhu`（🎃专注）
- **实体功能**：支持 119 个功能特性
- **状态**：可正常读取任务数据

#### 7. 关键信息确认 ✅
- **TickTick Project ID**：`69510f448f0805fa66144dc9`
  - 来源：Entity unique_id 和 TickTick 网页 URL 双重验证
  - 验证 URL：`https://ticktick.com/webapp/#b/69510f448f0805fa66144dc9/tasks`
- **标签机制**：TickTick 原生 Tags 功能
  - 测试任务 "🍅测试" 带有 `focus` 标签
  - 标签显示在 TickTick 网页版右侧（蓝色标签）
  - 左侧边栏显示 "标签" 区域有 `focus` 标签（1 个任务）

---

## ✅ 当前已落地（可直接用）

- 任务来源：`todo.zhuan_zhu`（🎃专注清单），通过 `todo.get_items` 拉取并筛选 `needs_action`
- 推送链路：HA → Focus Dial：`POST http://192.168.15.166/api/tasklist`
- 统计链路：Focus Dial → HA webhook → `input_text.focus_dial_stats_today`（自动化内 Jinja 合并 JSON，不用 `python_script`）
- 回写链路：收到 `task_done_decision` 且 `mark_task_done=true` → `ticktick.complete_task`（需用真实 TickTick 任务 `uid`）

---

## 📋 已落地的关键配置（可复制）

### **Phase 1: M1 - HA 推送任务列表到 Focus Dial**（✅ 90%：已落地，待设备端确认显示）

#### 目标：
从 TickTick 拉取“🎃专注清单”的待办任务（`needs_action`），组装成 JSON 推送到 Focus Dial 设备。

#### 需要配置的文件：

##### 1. `/home/zyh/homeassistant/configuration.yaml`
添加以下配置：

```yaml
# Focus Dial 任务列表推送
rest_command:
  focus_dial_push_tasklist:
    url: "http://192.168.15.166/api/tasklist"
    method: POST
    content_type: "application/json"
    payload: "{{ tasks_json }}"
    timeout: 5

# 今日累计统计存储
input_text:
  focus_dial_stats_today:
    name: "Focus Dial 今日统计"
    initial: "{}"
    # input_text 集成的长度上限为 255（超过会报错）
    max: 255
```

##### 2. `/home/zyh/homeassistant/scripts.yaml`
添加脚本：

```yaml
focus_dial_send_ticktick_focus_tasks:
  alias: "Focus Dial - 推送 TickTick 🎃专注清单任务"
  description: "读取 todo.zhuan_zhu 的待办任务，组装 JSON 推送到 Focus Dial"
  mode: single
  sequence:
    - variables:
        # 从统计实体读取今日累计
        stats: "{{ states('input_text.focus_dial_stats_today') | default('{}') | from_json }}"

        # TickTick 专注清单实体（直接用清单作为“过滤器”，不依赖 tags）
        ticktick_entity: "todo.zhuan_zhu"

    # 从 Todo 实体拉取 items（HA 侧标准方式）
    - service: todo.get_items
      target:
        entity_id: "{{ ticktick_entity }}"
      response_variable: todo_items_resp

    - variables:
        items: "{{ todo_items_resp[ticktick_entity]['items'] | default([]) }}"

        # 组装任务列表（设备优先显示 display_name；为避免 UTF-8 surrogate/Recorder 告警，name 也使用 ASCII）
        tasks_json: >
          {
            "tasks": [
              {% set ns = namespace(first=true) %}
              {% for item in items %}
                {% if item.status == 'needs_action' %}
                  {% set uid = (item.uid | default('') | string) %}
                  {% set raw = (item.summary | default('') | string | regex_replace('[\ud800-\udfff]', '')) %}
                  {% set ascii = (raw | regex_replace('[^a-zA-Z0-9# ]', '') | upper) %}
                  {% set suffix = uid[-4:] if (uid | length) >= 4 else uid %}
                  {% set display = ascii if (ascii | length) > 0 else ('TASK ' ~ suffix) %}
                  {% if not ns.first %},{% endif %}
                  {% set ns.first = false %}
                  {
                    "id": {{ uid | tojson }},
                    "name": {{ display | tojson }},
                    "display_name": {{ display | tojson }},
                    "duration": 25,
                    "spent_today_sec": {{ stats.get(uid, 0) | int }}
                  }
                {% endif %}
              {% endfor %}
            ]
          }

    - service: rest_command.focus_dial_push_tasklist
      data:
        tasks_json: "{{ tasks_json }}"

    - service: system_log.write
      data:
        level: info
        message: "Focus Dial: pushed {{ (tasks_json | from_json).tasks | length }} tasks"
```

**说明**：此方案不依赖 `tags` 字段，直接以 “🎃专注” 清单作为任务来源。

##### 3. Focus Dial 设备 IP 地址更新
- **当前配置**：`http://10.162.72.76/api/status`（旧配置，建议移除/替换）
- **实际设备 IP**：`192.168.15.166`（已通过 `GET /api/status` 验证）
- **需要操作**：
  1. 更新 configuration.yaml 中的 Focus Dial URL 为 `192.168.15.166`
  2. 推送任务列表使用：`POST http://192.168.15.166/api/tasklist`

#### 测试步骤：
1. 在 HA 开发者工具 → 服务，调用 `script.focus_dial_send_ticktick_focus_tasks`
2. 查看 HA 日志确认推送成功
3. 在 Focus Dial 设备空闲界面双击按键，查看任务列表是否显示

#### 预计耗时：30 分钟
#### 完成标志：Focus Dial 设备能显示 TickTick “🎃专注”清单的待办任务

---

### **Phase 2: M2 - 今日累计统计**（✅ 100%：已验证可累计/可每日清空）

#### 目标：
Focus Dial 完成专注计时后，通过 webhook 上报到 HA，累计每个任务今日已专注的秒数。

#### 实现要点（本轮最终方案）：
- 不使用 `python_script`（HA 的 `python_script` 环境限制较多，且禁止 `import json`）
- 直接在自动化里用 Jinja 把 JSON “读出 → 合并 → 写回”到 `input_text.focus_dial_stats_today`
- 注意：`input_text` 长度上限为 **255**，任务很多/ID 很长时可能溢出（后续建议迁移到更合适的存储）

##### `/home/zyh/homeassistant/automations.yaml`（关键片段）

```yaml
# 每日清空统计
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

# Webhook 接收事件并累计统计
- id: focus_dial_webhook_focus_completed
  alias: "Focus Dial - 累计专注统计"
  mode: queued
  max: 10
  trigger:
    - platform: webhook
      webhook_id: focus_dial
      allowed_methods: [POST]
  variables:
    payload: "{{ trigger.json }}"
    stats_entity: "input_text.focus_dial_stats_today"
    stats: "{{ (states(stats_entity) | default('{}')) | from_json | default({}) }}"
  condition:
    - condition: template
      value_template: >
        {{ payload.event == 'focus_completed'
           and (payload.count_time | default(false))
           and (payload.task_id | default('')) != ''
           and (payload.elapsed_seconds | default(0) | int) > 0 }}
  action:
    - variables:
        task_id: "{{ payload.task_id }}"
        add_seconds: "{{ payload.elapsed_seconds | int }}"
        next_total: "{{ (stats.get(task_id, 0) | int) + add_seconds }}"
        new_stats: "{{ stats | combine({ task_id: next_total }) }}"
    - service: input_text.set_value
      data:
        entity_id: "{{ stats_entity }}"
        value: "{{ new_stats | to_json }}"
    - service: system_log.write
      data:
        message: "Focus Dial: +{{ add_seconds }}s to {{ task_id }} (today={{ next_total }}s)"
        level: info
```

#### 快速自测（可选）
在电脑上模拟设备上报：
```bash
curl -sS -X POST "http://192.168.15.194:8123/api/webhook/focus_dial" \
  -H "Content-Type: application/json" \
  -d '{"event":"focus_completed","count_time":true,"task_id":"test_task","elapsed_seconds":60}'
```

#### 完成标志：
`input_text.focus_dial_stats_today` 会变成类似 `{"test_task":60}`，再次上报会累加。

---

### **Phase 3: M3 - 自动回写 TickTick**（✅ 80%：已落地，待用真实任务 UID 端到端验证）

#### 目标：
用户在 Focus Dial 选择标记任务完成后，HA 自动调用 TickTick API 完成任务。

#### 需要配置的文件：

##### 1. `/home/zyh/homeassistant/automations.yaml`
添加自动化：

```yaml
# Webhook 接收任务完成决策并回写 TickTick
- id: focus_dial_webhook_task_done_decision
  alias: "Focus Dial - 回写 TickTick 任务完成"
  description: "处理 Focus Dial 上报的 task_done_decision 事件"
  mode: queued
  max: 10
  trigger:
    - platform: webhook
      webhook_id: focus_dial
      allowed_methods:
        - POST
  variables:
    payload: "{{ trigger.json }}"
    ticktick_project_id: "69510f448f0805fa66144dc9"
  condition:
    - condition: template
      value_template: >
        {{ payload.event == 'task_done_decision'
           and (payload.mark_task_done | default(false))
           and (payload.task_id | default('')) != '' }}
  action:
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

        - service: system_log.write
          data:
            message: "Focus Dial: Failed to complete TickTick task {{ payload.task_id }}: {{ complete_response.error | default('Unknown') }}"
            level: error
```

#### 前置条件：
- ✅ TickTick 集成已安装并认证成功
- ✅ `ticktick.complete_task` 服务可用
- ✅ TickTick Project ID 已确认

#### 测试步骤：
0. 先从 `todo.zhuan_zhu` 的 items 里拿到一个真实任务 `uid`（`todo.get_items` 的返回里有）
1. 在 Focus Dial 完成一次计时
2. 选择 YES（标记完成）
3. 打开 TickTick App，确认对应任务已变为完成
4. 如果失败，HA 会显示 persistent notification，并可在日志看到 `complete_response` 的错误

#### 预计耗时：10 分钟
#### 完成标志：Focus Dial 选择 YES 后，TickTick 任务自动完成

---

## 🚨 已解决/仍需注意的点

### **1) `homeassistant.local` NXDOMAIN（已规避）**

- 已把 HA `homeassistant.internal_url/external_url` 固定为 `http://192.168.15.194:8123`，OAuth/跳转链接不再依赖 `.local`（mDNS）。

### **2) TickTick tags 不可用（已决策）**

- 不依赖 TickTick tags；以清单 `todo.zhuan_zhu`（🎃专注）作为“过滤器”，推送其 `needs_action` 任务。

### **3) `input_text` 长度上限（仍需注意）**

- `input_text` 的值最大 255 字符；任务多/ID 长时，统计 JSON 可能溢出（后续建议迁移到更合适的存储）。

---

## 📅 明早可直接用：快速检查清单（3 分钟）

1. 打开 HA：`http://192.168.15.194:8123`（统一用 IP）
2. 任务列表默认会自动推送（HA 启动后延迟约 20 秒 + 每 15 分钟刷新）；如需立即推送（二选一）：
   - HA → Developer Tools → Services 调用 `script.focus_dial_send_ticktick_focus_tasks`
   - 或直接触发：`POST http://192.168.15.194:8123/api/webhook/focus_dial_push_now_3a8f63cf`
3. 设备端刷新任务：空闲界面双击，确认任务列表出现
4. 做一次专注：结束后查看 `input_text.focus_dial_stats_today` 是否累加
5. 选择 YES 标记完成：确认 TickTick 任务是否被自动完成（失败会弹 HA 通知）

---

## 📝 配置文件位置总结

| 文件路径 | 用途 | 状态 |
|---------|------|------|
| `/home/zyh/homeassistant/configuration.yaml` | HA 主配置文件（`internal_url/external_url` + `rest_command` + `input_text`） | ✅ 已应用 |
| `/home/zyh/homeassistant/scripts.yaml` | HA 脚本配置（推送任务列表） | ✅ 已添加 |
| `/home/zyh/homeassistant/automations.yaml` | HA 自动化配置（webhook + 统计 + 回写） | ✅ 已添加 |
| `/home/zyh/homeassistant/python_scripts/focus_dial_add_seconds.py` | Python 累计脚本 | ❌ 不再需要（已改为自动化内 Jinja 累计） |
| `/home/zyh/homeassistant/python_scripts/test_ticktick_data.py` | 测试脚本（查看任务数据） | ✅ 已创建 |
| `/home/zyh/homeassistant/custom_components/ticktick/` | TickTick 集成 | ✅ 已安装 |
| `/home/zyh/homeassistant/custom_components/hacs/` | HACS 集成 | ✅ 已安装 |
| `/home/zyh/homeassistant/.storage/application_credentials` | OAuth 凭据存储 | ✅ 已配置 |

---

## 🔑 关键信息汇总

### **TickTick 信息**
- **Client ID**：`hTqUw97rjPwspw8J4X`
- **Client Secret**：`******（已脱敏，见 TickTick Developer 控制台）`
- **Project ID**（🎃专注清单）：`69510f448f0805fa66144dc9`
- **Entity ID**：`todo.zhuan_zhu`
- **标签机制**：TickTick 原生 Tags（`focus` 标签）

### **Home Assistant 信息**
- **访问地址**：http://192.168.15.194:8123 或 http://localhost:8123
- **配置目录**：`/home/zyh/homeassistant/`
- **Docker 容器名**：`homeassistant`
- **事件 Webhook（Focus Dial → HA）**：`focus_dial`（兼容：`focusdial_910772ca41bc61b2fbf94c466f74d729`）
- **事件 Webhook URL**：`http://192.168.15.194:8123/api/webhook/focus_dial`
- **调试推送 Webhook（手动触发 M1 推送）**：`focus_dial_push_now_3a8f63cf`
- **调试推送 URL**：`http://192.168.15.194:8123/api/webhook/focus_dial_push_now_3a8f63cf`

### **Focus Dial 信息**
- **设备 IP**：`192.168.15.166`
- **API 端点**：
  - `/api/tasklist` - 接收任务列表
  - `/api/status` - 查询设备状态
  - （不支持）`/api/start`、`/api/stop`（当前固件返回 404）

---

## 📖 参考文档

- [HA-TICKTICK-SETUP-GUIDE.md](/home/zyh/Desktop/focus-dial-main/HA-TICKTICK-SETUP-GUIDE.md) - OAuth 配置指南
- [ha-ticktick-focusdial-config.yaml](/home/zyh/Desktop/focus-dial-main/ha-ticktick-focusdial-config.yaml) - 完整配置模板
- [hantick-ticktick-integration-analysis.md](/home/zyh/Desktop/focus-dial-main/hantick-ticktick-integration-analysis.md) - TickTick 集成分析
- [FOCUS_DIAL_TICKTICK_HA_总结.md](/home/zyh/Desktop/focus-dial-main/FOCUS_DIAL_TICKTICK_HA_总结.md) - 项目背景总结
- [HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md](/home/zyh/Desktop/focus-dial-main/HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md) - 技术方案文档
- [ROADMAP_TickTick_HA_FocusDial.md](/home/zyh/Desktop/focus-dial-main/ROADMAP_TickTick_HA_FocusDial.md) - 项目路线图

---

## 🎯 总结

### **已完成**（≈80%）
✅ TickTick OAuth 认证成功（已规避 `.local` 问题）  
✅ HA → Focus Dial 任务列表推送已落地（`todo.get_items` → `/api/tasklist`）  
✅ Focus Dial → HA 今日统计累计已验证（webhook + Jinja 合并 JSON）  
✅ 回写 TickTick 自动化已落地（`ticktick.complete_task`）

### **仍需你明早肉眼确认的两件事**
1) 设备端界面是否正确刷新并显示任务列表（推送后双击刷新）  
2) 用“真实任务 UID”完成一次 `task_done_decision` 回写，确认 TickTick 任务变为完成  

---

**浮浮酱在这里等主人的指示喵～** ฅ'ω'ฅ

有任何问题随时告诉浮浮酱喵～ φ(≧ω≦*)♪

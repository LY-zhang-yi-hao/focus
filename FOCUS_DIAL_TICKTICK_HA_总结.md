# Focus Dial × TickTick（#focus）× Home Assistant（HA）联动（方案 B）总结

更新时间：2025-12-28  
适用范围：你这套“HA 作为中间层”的联动方案（TickTick → HA → Focus Dial），并支持“设备显示 + HA 面板显示 + 自动回写 TickTick”。

---

## 0. 你的当前环境（真实局域网信息）

- Focus Dial（ESP32，设备名：`esp32-8412D4`）
  - IP：`192.168.15.166`
  - MAC：`64:B7:08:84:12:D4`
  - HTTP API：`http://192.168.15.166`（端口 80）
- 你的手机（三星 / Android 16，设备名：`shi-dao`）
  - IP：`192.168.15.172`
  - MAC：`8E:63:83:92:A3:4A`
- 你的电脑（`ubun`，也部署了 Home Assistant）
  - IP：`192.168.15.194`
  - MAC：`F4:7B:09:4B:0E:A1`
- 路由器（X-WRT）
  - LAN 网关：`192.168.15.1`
- DHCP 静态租约/地址保留：已配置为 infinite（地址长期稳定）

---

## 1. 目标与边界（你明确的产品需求）

### 1.1 你要实现什么

1) 设备端能选择你每天规定的 TickTick 任务（仅 `#focus`）。  
2) 设备端可对“选中的任务”进行计时（番茄钟），并把事件上报给 HA。  
3) 统计口径：  
   - **只有“正常结束”的专注才计入该任务的今日累计时长**  
   - **暂停/取消不计入该任务**  
4) 每次番茄钟“结束”或“中途取消”，设备弹窗询问：**是否已完成该任务**。  
5) HA 侧：  
   - 面板显示今日累计（按任务）  
   - 接收设备事件后，自动回写 TickTick（把用户选择“已完成”的任务标记完成）

### 1.2 你选择的架构方案

- **方案 B（HA 中间层）**：TickTick 的数据由 HA 获取与聚合，设备只做展示/选择/计时与上报。

---

## 2. 总体架构与数据流（方案 B）

### 2.1 任务列表下发：TickTick → HA → Focus Dial

1) HA 从 TickTick 拉取任务（过滤 `#focus`）。  
2) HA 将每个任务拼装为设备可识别的结构，并补充“今日累计秒数”。  
3) HA 调用设备 HTTP API：`POST http://192.168.15.166/api/tasklist` 下发任务列表。  
4) Focus Dial 显示任务列表，用户旋钮选择，按键开始计时。

### 2.2 计时事件上报：Focus Dial → HA webhook → 统计/回写 TickTick

1) Focus Dial 计时开始/暂停/取消/完成会向 HA webhook 发送 JSON。  
2) HA 根据事件：
   - `focus_completed` 且 `count_time=true`：累计到“今日统计”
   - `focus_paused` / `focus_canceled`：不累计
3) 在“结束/取消”后设备会再发一条 `task_done_decision`：  
   - `mark_task_done=true`：HA 回写 TickTick，把该任务标记完成

---

## 3. 设备端交互（Focus Dial）

### 3.1 任务列表入口

- **空闲界面双击按键**：进入任务列表（`SELECT TASK`）
  - 如果还没收到 HA 下发：显示 `NO TASKS` / `PUSH FROM HA`

### 3.2 任务列表操作

- 旋钮：上下选择任务  
- 按键：确认并按该任务建议时长开始计时  
- 长按：返回空闲

> 任务列表底部显示：  
> - `Dxxm`：建议专注时长（分钟）  
> - `Txxm` 或 `Txxhmm`：今天该任务累计专注时长（来自 HA 下发的 `spent_today_sec`）

### 3.3 结束/取消后的“是否完成任务”

- 正常结束或取消后，设备弹窗：`MARK DONE?`（YES/NO）
- 选择并确认后，设备上报 `task_done_decision` 给 HA（由 HA 决定是否回写 TickTick）

---

## 4. 设备 HTTP API（HA → 设备）

### 4.1 查看设备状态

```bash
curl --max-time 5 "http://192.168.15.166/api/status"
```

返回字段说明：
- `wifi_connected`：WiFi 是否已连接
- `tasklist_loaded`：本次开机后是否成功接收过一次有效 `/api/tasklist`（JSON 校验通过后才会置为 true）

### 4.2 下发任务列表

```bash
curl --max-time 5 -X POST "http://192.168.15.166/api/tasklist" \
  -H "Content-Type: application/json" \
  -d '{
    "tasks":[
      {"id":"ticktick:1","name":"学习 Rust","display_name":"学习 Rust","status":"needs_action","duration":25,"spent_today_sec":120}
    ]
  }'
```

字段说明：
- `id`：任务唯一 ID（建议用 TickTick task_id 或前缀化 `ticktick:...`）
- `name`：任务名称（固件已支持中文字体，可直接显示中文）
- `display_name`：设备显示名（可选，兼容字段）。通常可与 `name` 相同，不再要求拼音/ASCII。
- `status`：任务状态：`needs_action`（待办）/ `completed`（已完成），用于设备端双击切换查看两类列表。
- `duration`：建议专注时长（分钟）
- `spent_today_sec`：今日累计专注秒数（由 HA 统计后下发，设备只负责展示）

---

## 5. 设备 → HA webhook 事件（Focus Dial → HA）

Focus Dial 会向你在配网页面填写的 `Webhook URL` 发送 `POST application/json`。

### 5.1 关键事件与统计口径

- `focus_completed`：**计入今日统计**（`count_time=true`）
- `focus_paused`：不计入
- `focus_canceled`：不计入
- `task_done_decision`：用户选择是否“标记 TickTick 完成”

> 完整字段样例与 HA 自动化模板：见 `HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md`

---

## 6. 中文任务名显示问题（蓝条/无文字）——更新与结论

### 6.0 更新（2025-12-29）

- ✅ 已实现“设备端引入中文字体/点阵渲染”：固件集成 `U8g2_for_Adafruit_GFX` + `u8g2_font_wqy12_t_gb2312`，OLED 可原生显示中文任务名与全中文界面文案。
- ✅ `display_name` 仅保留为兼容字段，**不再推荐**依赖拼音/ASCII。

### 6.1 现象描述（你现场观察）

- `SELECT TASK` 界面出现“整行蓝色高亮条”，但看不到任务文字。  
- 这类现象最常见原因：**当前屏幕使用的英文字库不包含中文字符**，中文会渲染为空白，从而只剩“高亮条”。

### 6.2 快速验证（10 秒确认是不是字体问题）

向设备下发一个纯英文任务名：

```bash
curl --max-time 5 -X POST "http://192.168.15.166/api/tasklist" \
  -H "Content-Type: application/json" \
  -d '{"tasks":[{"id":"1","name":"TEST #focus","duration":25,"spent_today_sec":0}]}'
```

- 若能显示 `TEST #focus`：基本确认“中文字体不支持”是主因  
- 若仍不显示：再检查 `/api/status` 是否 `tasklist_loaded:true`，以及 HA 下发 JSON 是否有效（第 7 节排查）

### 6.3 方案 1：`display_name` / 拼音简称（已不推荐，仅兼容保留）

**思路**：HA 下发任务时同时提供一个“设备友好显示名”（ASCII/英文/拼音/简称），设备优先显示 `display_name`，缺省再回退到 `name`。

- 优点
  - 改动小、风险低（不需要引入大字体）
  - 显示稳定、性能好
  - 允许 HA 做更多策略：拼音、缩写、编号、优先级、emoji 替换等
- 缺点
  - 设备屏幕上看到的是“简称/拼音”，不是原生中文

**推荐做法**：
- HA 侧：给每个任务计算 `display_name`（例如“写周报 #focus”→“XZB #focus”或“xie zhou bao”）
- 固件侧：增加字段支持（后续待办）
  - 任务结构新增 `display_name`
  - `drawTaskListScreen()` 优先显示 `display_name`

> 这是当前最“工程可控”的路径：先把功能跑通，再决定是否值得上中文字体。

### 6.4 方案 2：设备端引入中文字体/点阵渲染（已落地）

**思路**：在固件中加入可渲染中文的字体（点阵字库或更换显示库/字体引擎），直接显示 `name` 的中文。

- 优点
  - 设备端显示原生中文，体验最佳
- 缺点/风险
  - 字库体积大（Flash/RAM 占用上涨），编译/OTA 体积压力更大
  - 渲染开销更高，滚动/刷新可能变慢
  - 引入新库（如 U8g2）可能需要改动现有显示代码与资源

**适用场景**：
- 你强依赖设备端显示中文完整标题（而不是简称）
- 你能接受更大的固件与更复杂的维护成本

### 6.5 推荐结论

- **当前推荐：方案 2（已落地）**。设备端直接显示 `name` 的中文，体验更一致；待办/已完成也能在设备端切换查看。

---

## 7. 常见问题排查清单（按优先级）

### 7.1 `tasklist_loaded:false` 但你认为“已经推送了”

含义：设备本次开机后没有成功接收一次“JSON 校验通过”的 `/api/tasklist`。

请按顺序检查：
1) 设备能否访问（同网段）：
   - `curl --max-time 5 http://192.168.15.166/api/status`
2) 推送是否返回 200：
   - `curl -sv --max-time 5 -X POST http://192.168.15.166/api/tasklist -H 'Content-Type: application/json' -d '{"tasks":[]}'`
3) HA 模板是否生成了合法 JSON（常见坑：多余逗号、字符串转义、tasks 不是数组）

### 7.2 任务列表出现高亮条但无文字

若仍出现“高亮条但无文字”：
1) 确认已烧录本次更新后的固件（已集成中文字库）
2) 确认下发 JSON 为合法 UTF-8（避免异常转义/非法字符）

### 7.3 HA 下发任务为空（设备显示 NO TASKS）

检查 HA 里 TickTick 任务实体是否真的有 `#focus` 任务；以及过滤条件是否正确。

---

## 8. 下一步落地清单（建议顺序）

1) 先用 `curl` 确认设备端链路 OK（`/api/status`、`/api/tasklist`）  
2) HA 侧落地：
   - 定义 `rest_command` 推送任务列表到 `192.168.15.166/api/tasklist`
   - webhook 自动化：接收 `focus_completed` 累计今日 seconds
   - webhook 自动化：接收 `task_done_decision` 且 `mark_task_done=true` 时回写 TickTick
3) 处理中文显示：
   - 首推 `display_name`（HA 生成简称/拼音）  
   - 视需要再做“中文字体”增强

参考实现（更详细）：`HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md`

---

## 9. 手机“工作模式”触发（三星 / Android 16）

你现在的“手机进入工作模式”建议走 **蓝牙** 触发（更直接、更轻量）；webhook 则继续用于 HA 的灯光/网络屏蔽/任务统计等。

### 9.1 两条链路的区别（你可以同时用，也可以只用其一）

- 蓝牙链路（推荐用于手机模式）
  - 触发源：手机与 `Focus Dial` 的 **连接/断开**
  - 动作：三星「模式与例程」开启/关闭“工作模式”
  - 优点：不依赖 HA，不怕 HA 暂时不可用
- Webhook 链路（推荐用于家庭自动化/统计）
  - 触发源：Focus Dial 通过 Wi-Fi 向 `Webhook URL` 发送 `POST JSON`
  - 动作：HA 自动化（灯光、网络、TickTick 统计与回写等）

### 9.2 推荐设置（所有专注统一用“工作模式”）

在三星「模式与例程」里创建两条例程（或一条带“结束时恢复”的例程）：

1) 当 **蓝牙连接** `Focus Dial` → **开启**「工作模式」  
2) 当 **蓝牙断开** `Focus Dial` → **关闭**「工作模式」（或恢复上一个模式）

> 你提到“暂停时工作模式关闭”：当前固件会在暂停时断开蓝牙，因此只要你的例程按“断开蓝牙关闭工作模式”配置，就能满足需求。

### 9.3 如果你“计时了但看不到蓝牙/例程不触发”

这版固件的蓝牙并不是一直广播：只有在**已配对**（设备侧 `bt_paired=true`）后，计时开始才会拉起蓝牙连接。

排查顺序：
1) 手机蓝牙设置里是否存在“已配对设备：`Focus Dial`”
2) 计时开始时手机是否会自动连接该设备（连接后工作模式应被触发）
3) 若从未配对过：需要先让设备进入配网/配对流程完成一次配对（后续可考虑加“仅蓝牙配对模式（不清 Wi-Fi）”作为固件增强项）

---

## 10. Home Assistant 访问与地址稳定性（你这次遇到的“同 Wi-Fi 但连不上”）

### 10.1 你当前的 HA 访问地址

- HA：`http://192.168.15.194:8123`

快速自测（任意一台同网段设备执行）：

```bash
curl --max-time 3 -I "http://192.168.15.194:8123/"
```

### 10.2 手机 App 仍连不上时，优先检查“内部地址”

你这次的根因是：HA App 内部地址仍指向旧 IP（例如 `10.162.72.83:8123`），导致 `ERR_CONNECTION_ABORTED`。

处理原则：
- 以当前可达的内网地址为准（本方案推荐固定 `192.168.15.194:8123`）
- 如果未来 HA 主机 IP 变化，再同步更新 App 内部地址即可

### 10.3 让 IP 不再漂移（已完成）

你已在 X-WRT 的 `DHCP/DNS → 静态地址分配` 配置了地址保留（infinite）：
- `ubun` → `192.168.15.194`
- `esp32-8412D4` → `192.168.15.166`

### 10.4 有线 + Wi‑Fi 同时开启会不会影响？

可能影响“你该用哪个地址访问”：
- 同时开启时，电脑可能会同时拥有**有线 IP** + **Wi‑Fi IP**；手机在 Wi‑Fi 下访问时，优先使用同网段可达的那个 IP。
- 想保持“永远用一个地址”：
  - 最简单：让电脑 Wi‑Fi 始终保持连接（继续用 `192.168.15.194`）
  - 更稳：把电脑**有线网卡的 MAC** 也在路由器里做一条静态租约，并在 HA App 里备用一个内部地址（如果你确实会切换到只用有线）

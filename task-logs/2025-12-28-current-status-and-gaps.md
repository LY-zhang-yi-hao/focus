# 当前完成状态与不足（TickTick × Home Assistant × Focus Dial）

**更新时间**：2025-12-28  
**目标**：确保你明天早上来可以直接使用（任务下发、设备可选任务、计时累计、可回写完成）。

---

## 一、当前可用环境（关键常量）

- **Home Assistant**：`http://192.168.15.194:8123`（已规避 `homeassistant.local` / mDNS）
- **Focus Dial**：`http://192.168.15.166`
  - ✅ `GET /api/status`
  - ✅ `POST /api/tasklist`
- **TickTick（Hantick 集成）**
  - Todo 实体：`todo.zhuan_zhu`（🎃专注清单）
  - Project ID：`69510f448f0805fa66144dc9`

---

## 二、完成状态（按里程碑）

### M0：OAuth / 集成接入（✅ 已完成）

- TickTick OAuth 已认证成功，redirect URL 以 IP 形式固定，避免 `.local` 打不开
- Client Secret 已在仓库文档中脱敏（避免泄漏）

### M1：HA → Focus Dial 下发任务列表（✅ 已完成，设备已加载）

- 下发链路：HA `rest_command.focus_dial_push_tasklist` → `POST http://192.168.15.166/api/tasklist`
- 任务来源：`todo.get_items` 拉取 `todo.zhuan_zhu`，筛选 `needs_action`
- **已修复关键阻塞**：下发 JSON 模板曾混入 `# 注释` 导致设备端 400，现在已清除
- **验证结果**：设备 `GET /api/status` 显示 `tasklist_loaded:true`
- **可靠性增强**：
  - HA 启动后延迟约 20 秒自动推送一次任务列表
  - 每 15 分钟周期推送一次（设备在线才推）

### M2：Focus Dial → HA 今日累计统计（✅ 已完成，已验证可累加）

- 上报方式：Focus Dial webhook → HA 自动化
- 存储：`input_text.focus_dial_stats_today`（JSON 字典：`{task_id: seconds}`）
- 实现：自动化内 Jinja（`combine + to_json`）合并累计，不依赖 `python_script`
- 已配置：每日 0 点清空
- **验证方式**：可用 `curl` 模拟 `focus_completed` 上报，观察统计累加

### M3：Focus Dial → TickTick 自动回写完成（✅ 已落地，待真实 UID 端到端确认）

- 已配置：收到 `task_done_decision` 且 `mark_task_done=true` → 调用 `ticktick.complete_task`
- **仍需明早确认**：用真实 TickTick 任务 `uid`（来自 `todo.get_items` 的 item.uid）在设备上完成一次“YES”，确认 TickTick 任务变为完成

---

## 三、明早 3 分钟验收步骤（建议照做）

1. 打开 HA：`http://192.168.15.194:8123`
2. 设备空闲界面双击刷新任务列表（HA 已自动推送；也可手动触发推送）
3. 选一个任务开始计时 → 正常结束
4. 回到 HA 查看 `input_text.focus_dial_stats_today` 是否累加
5. 弹窗选择 YES 标记完成 → 打开 TickTick 确认任务已完成

手动触发推送（二选一）：
- HA → Developer Tools → Services 调用 `script.focus_dial_send_ticktick_focus_tasks`
- 或：`POST http://192.168.15.194:8123/api/webhook/focus_dial_push_now_3a8f63cf`

---

## 四、当前不足 / 风险点（以及建议）

1) **设备只能稳定显示可打印 ASCII**
- 现状：固件会对非 ASCII 文本做兜底显示（避免中文渲染为空白）
- 影响：TickTick 任务如果全是中文，设备上可能只看到类似 `TASK 7BEA` 的编号
- 建议（最省事）：在 TickTick 任务标题前加一个短 **ASCII 前缀**，例如：`R1 阅读`、`C1 CODING`、`HA1 部署`，设备就能显示前缀并便于选择

2) **`input_text` 长度上限 255**
- 现状：统计 JSON 存在 `input_text.focus_dial_stats_today`，任务多/ID 长时可能溢出
- 建议：后续升级为 `pyscript/Node-RED/自建存储`，或只保留“今日被使用过的 TopN 任务”

3) **webhook 重试导致重复累计（尚未做去重）**
- 现状：网络抖动/设备重试可能导致同一 session 重复上报，从而重复累加
- 建议：利用 payload 里的 `session_id` 做去重（持久化已处理 session_id 列表）

4) **安全性（webhook 无鉴权）**
- 现状：为了“明早即用”，提供了本地无鉴权的推送 webhook（`local_only:true` 已限制本机/局域网）
- 建议：不要对外网暴露 HA 8123；如需外网访问，建议走反向代理 + 鉴权，并禁用调试 webhook

5) **网络环境变化（IP 变动）**
- 现状：全链路以 IP 固定（规避 `.local`），但路由器 DHCP 变动会导致地址变化
- 建议：给 HA 主机与 Focus Dial 设备设置 DHCP 静态租约（固定 IP）

---

## 五、仓库内同步情况（文档已更新）

- `TICKTICK-HA-INTEGRATION-STATUS-2025-12-28.md`：已同步到“实机已落地”方案 + 明早验收清单
- `ha-ticktick-focusdial-config.yaml`：已同步最终方案（`todo.get_items` + 自动化内 Jinja 累计）
- `HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md`、`HA-TICKTICK-SETUP-GUIDE.md`、`ROADMAP_TickTick_HA_FocusDial.md`：已同步与脱敏


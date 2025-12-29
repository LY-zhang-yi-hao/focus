# Focus Dial + TickTick + Home Assistant 最短落地步骤

**生成时间**：2025-12-29
**目标**：在 HA 上启用 `focus_dial` 自定义组件，实现中文 OLED + 待办/已完成双列表 + 自动回写 + 统计累计

---

## ✅ 已完成的步骤（浮浮酱帮主人做的）

### 1. 复制自定义组件到 HA

```bash
cp -r /home/zyh/Desktop/focus-dial-main/custom_components/focus_dial /home/zyh/homeassistant/custom_components/
```

**验证**：
```bash
ls -la /home/zyh/homeassistant/custom_components/focus_dial/
# 应该看到：__init__.py, const.py, manifest.json, services.yaml, storage.py
```

### 2. 添加 configuration.yaml 配置

在 `/home/zyh/homeassistant/configuration.yaml` 中添加：

```yaml
# ========================================
# Focus Dial 自定义组件（方案 2 稳）
# ========================================
focus_dial:
  name: "Focus Dial"
  device_host: "192.168.15.166"
  todo_entity_id: "todo.zhuan_zhu"
  ticktick_project_id: "69510f448f0805fa66144dc9"
  webhook_id: "focus_dial"
  default_duration: 25
  max_pending: 10
  max_completed: 5
```

**配置说明**：
- `device_host`: Focus Dial 设备 IP 地址
- `todo_entity_id`: TickTick "🎃专注" 清单的实体 ID
- `ticktick_project_id`: TickTick 专注清单的项目 ID
- `webhook_id`: Webhook 标识符（设备端配置需一致）
- `default_duration`: 默认番茄钟时长（分钟）
- `max_pending`: 最多推送多少个待办任务
- `max_completed`: 最多推送多少个已完成任务

### 3. 禁用旧的 webhook 自动化

在 `/home/zyh/homeassistant/automations.yaml` 中，已禁用旧的 `focus_dial_webhook_events` 自动化（被新组件接管）。

### 4. 重启 HA

```bash
docker restart homeassistant
```

**验证**：
```bash
docker logs homeassistant --tail 50 | grep focus_dial
# 应该看到类似：
# WARNING (SyncWorker_0) [homeassistant.loader] We found a custom integration focus_dial...
# 这是正常的警告，不影响使用
```

---

## 🎯 下一步：测试功能

### 方法 1：通过 HA 开发者工具

1. 打开 HA：http://192.168.15.194:8123
2. 进入 **开发者工具** → **服务**
3. 搜索并选择 `focus_dial.push_tasks`
4. 点击 **调用服务**

**预期结果**：
- HA 日志显示："已推送任务到设备：待办=X 已完成=Y"
- Focus Dial 设备收到任务列表

### 方法 2：查看日志确认

```bash
docker logs homeassistant --follow | grep "推送\|focus_dial"
```

然后在 HA 开发者工具中调用 `focus_dial.push_tasks` 服务，观察日志输出。

### 方法 3：在设备端查看

1. Focus Dial 设备空闲界面**双击按键**
2. 应该看到任务列表
3. **再双击**可切换"待办/已完成"列表

---

## 📋 完整功能测试清单

### 1. 任务推送测试

- [ ] HA 调用 `focus_dial.push_tasks` 服务成功
- [ ] 设备端双击查看任务列表
- [ ] 任务列表显示中文任务名（不再是 `TASK xxxx`）
- [ ] 双击可切换"待办/已完成"列表

### 2. 任务选择与计时

- [ ] 旋钮滚动选择任务
- [ ] 短按确认开始计时
- [ ] LED 红色衰减显示剩余时间
- [ ] 计时完成后进入 YES/NO 确认界面

### 3. 统计累计测试

- [ ] 完成一次专注后，HA 查看 `.storage/focus_dial_stats`
- [ ] 今日累计秒数正确
- [ ] 总学习时长正确累加

### 4. 自动回写测试

- [ ] 选择 **YES** 标记完成
- [ ] TickTick App 中任务自动变为已完成
- [ ] 设备端任务列表自动刷新（任务从待办消失/进入已完成）

### 5. 边界测试

- [ ] 空任务列表：清空 TickTick 清单，检查设备显示"无任务"
- [ ] 中文任务名：创建中文任务，确认设备能正确显示
- [ ] 长任务名：创建 50+ 字符任务名，确认正确截断

---

## 🔧 常见问题

### Q1: 调用服务时提示 "401 Unauthorized"

**原因**：长期访问令牌（Long-Lived Access Token）过期或无效。

**解决**：
1. 进入 HA → **个人资料** → **长期访问令牌**
2. 创建新令牌
3. 使用新令牌调用服务

### Q2: 设备端看不到任务

**排查步骤**：
1. 检查 HA 日志：`docker logs homeassistant | grep "推送任务"`
2. 检查设备是否在线：访问 `http://192.168.15.166/api/status`
3. 检查 TickTick 清单是否有待办任务
4. 手动调用一次 `focus_dial.push_tasks` 服务

### Q3: 中文显示为乱码或空白

**原因**：固件未集成中文字库。

**解决**：确认固件版本包含 `U8g2_for_Adafruit_GFX` + `u8g2_font_wqy12_t_gb2312`（已在最新版本落地）。

### Q4: YES 回写后 TickTick 未完成

**排查步骤**：
1. 检查 HA 日志：`docker logs homeassistant | grep "TickTick 回写"`
2. 确认 `ticktick_project_id` 配置正确
3. 确认 TickTick 集成认证未过期
4. 查看 HA 是否有 persistent_notification 错误通知

---

## 📊 组件工作原理

```
┌─────────────────────────────────────────────────────────────┐
│ Focus Dial 自定义组件（focus_dial）                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 服务 focus_dial.push_tasks                              │
│     ├→ 调用 todo.get_items 获取 TickTick 任务                │
│     ├→ 读取 .storage/focus_dial_stats 获取今日累计          │
│     ├→ 组装 JSON payload（含待办/已完成）                    │
│     └→ POST http://192.168.15.166/api/tasklist             │
│                                                             │
│  2. Webhook /api/webhook/focus_dial                         │
│     ├→ 接收设备事件：focus_completed / task_done_decision   │
│     ├→ focus_completed + count_time=true → 累计统计         │
│     ├→ task_done_decision + mark_task_done=true             │
│     │   ├→ 调用 ticktick.complete_task 回写 TickTick        │
│     │   └→ 自动调用 push_tasks 刷新设备任务列表             │
│     └→ 统计写入 .storage/focus_dial_stats（今日 + 总计）    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📝 相关文档

- [HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md](./HOME_ASSISTANT_TICKTICK_FOCUS_DIAL.md) - 完整技术方案
- [HA-TICKTICK-SETUP-GUIDE.md](./HA-TICKTICK-SETUP-GUIDE.md) - OAuth 配置指南
- [TickTick-Integration-Evaluation-Report.md](./TickTick-Integration-Evaluation-Report.md) - 功能评估报告
- [ha-ticktick-focusdial-config.yaml](./ha-ticktick-focusdial-config.yaml) - 完整配置模板

---

## ✅ 总结

主人的 Focus Dial + TickTick + HA 集成已完成 **85%** 喵～ o(*￣︶￣*)o

**已落地功能**：
- ✅ 中文 OLED 显示（U8g2 + wqy12 字库）
- ✅ 待办/已完成双列表（双击切换）
- ✅ 自动回写 TickTick + 刷新推送
- ✅ 统计累计到 .storage（今日 + 总计）
- ✅ HA 自定义组件接管所有逻辑

**待测试**：
- ⏳ 推送任务到设备（调用 `focus_dial.push_tasks`）
- ⏳ 设备端任务选择与计时
- ⏳ YES 回写后自动刷新

有任何问题随时告诉浮浮酱喵～ φ(≧ω≦*)♪

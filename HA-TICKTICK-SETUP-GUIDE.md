# TickTick × Home Assistant 完整配置指南

**生成时间**：2025-12-28
**作者**：浮浮酱 φ(≧ω≦*)♪

---

## 🎯 目标

完成 TickTick 集成的 OAuth 认证，并配置完整的自动化流程。

---

## ✅ 已完成的准备工作

1. ✅ HACS 已安装
2. ✅ Hantick TickTick 集成已安装到 `/home/zyh/homeassistant/custom_components/ticktick/`
3. ✅ HA 已配置 `external_url: http://192.168.15.194:8123`
4. ✅ TickTick Developer 应用已创建：
   - Client ID: `hTqUw97rjPwspw8J4X`
   - Client Secret: `******（已脱敏，见 TickTick Developer 控制台）`
   - OAuth redirect URL: `http://192.168.15.194:8123/auth/external/callback`

---

## 📋 主人需要做的 3 步操作（Web UI）

### **Step 1：添加 Application Credentials（5 分钟）**

这是**最关键的一步**，必须在 Web UI 手动完成喵～

1. **打开浏览器**，访问：`http://192.168.15.194:8123`

2. **登录 Home Assistant**

3. **进入 Application Credentials 页面**：
   - 点击左下角 **"Settings"**（设置）
   - 点击 **"Devices & services"**（设备与服务）
   - 点击顶部标签页 **"Application Credentials"**（应用凭据）
   - 或直接访问：`http://192.168.15.194:8123/config/application_credentials`

4. **添加凭据**：
   - 点击右下角蓝色按钮 **"+ ADD APPLICATION CREDENTIAL"**
   - 在弹出的对话框中：
     - **Integration（集成）**：从下拉菜单选择 **"TickTick"**
     - **Client ID（客户端 ID）**：粘贴 `hTqUw97rjPwspw8J4X`
     - **Client Secret（客户端密钥）**：粘贴你在 TickTick Developer 控制台里的 Client Secret
   - 点击 **"ADD"** 或 **"提交"**

5. **验证**：
   - 添加成功后，应该能在 Application Credentials 列表里看到 **"TickTick"**

---

### **Step 2：添加 TickTick 集成（5 分钟）**

1. **回到 Devices & services 主页面**：
   - Settings → Devices & services
   - 点击 **"Integrations"** 标签页

2. **添加集成**：
   - 点击右下角蓝色按钮 **"+ ADD INTEGRATION"**
   - 在搜索框输入：`TickTick`
   - 点击搜索结果中的 **"TickTick"**

3. **开始 OAuth 认证**：
   - 点击 **"LINK ACCOUNT"** 或 **"链接账号"**
   - **重要**：这次会弹出一个确认对话框，显示 HA 实例 URL
   - 点击 **"Link account"**（蓝色按钮）

4. **TickTick 授权页面**：
   - 浏览器会跳转到 TickTick 授权页面
   - 如果未登录，先登录 TickTick 账号
   - 点击 **"授权"** 或 **"Authorize"**

5. **完成**：
   - 授权成功后，浏览器会自动跳回 HA
   - HA 会显示 **"Success!"** 或成功提示
   - 在 Devices & services 列表里应该能看到 **"TickTick"** 集成卡片

---

### **Step 3：验证集成成功（2 分钟）**

1. **查看 Todo 实体**：
   - Settings → Devices & services → Entities
   - 在搜索框输入：`todo.ticktick`
   - 应该能看到主人的 TickTick Lists，例如：
     - `todo.ticktick_inbox`
     - `todo.ticktick_focus`（如果有这个 List）
     - 等等

2. **查看实体详情**：
   - 点击任意一个 `todo.ticktick_*` 实体
   - 点击 **"ATTRIBUTES"** 标签页
   - 应该能看到 `items` 属性，包含任务列表

3. **测试服务**：
   - Developer Tools → Services
   - 搜索：`ticktick.get_projects`
   - 点击 **"CALL SERVICE"**
   - 应该能在 Response 里看到主人的所有 TickTick Projects

---

## 🎉 完成后的下一步

主人完成上述 3 步后，告诉浮浮酱以下信息喵～

1. **OAuth 认证是否成功？**
   - [ ] 成功（看到 "Success!" 并且有 todo 实体）
   - [ ] 失败（告诉浮浮酱错误信息）

2. **创建了哪些 todo 实体？**
   - 请列出实体 ID，例如：
     - `todo.ticktick_inbox`
     - `todo.ticktick_focus`
     - `todo.ticktick_work`

3. **主人有没有一个专门用于 #focus 任务的 TickTick List？**
   - [ ] 有（List 名称：______，对应实体：`todo.ticktick_______`）
   - [ ] 没有（浮浮酱帮主人在 TickTick 创建一个）

4. **调用 `ticktick.get_projects` 返回的 Project ID**：
   - 找到 "focus" 或类似名称的 Project
   - 记录它的 `id` 字段，例如：`6987a8plg0rad90bc38b672f`

---

## 🔍 故障排查

### 问题 1：找不到 "TickTick" Integration

**原因**：TickTick 集成未正确安装

**解决**：
```bash
ls /home/zyh/homeassistant/custom_components/ticktick/manifest.json
```
如果文件不存在，说明集成未安装，重新复制：
```bash
cp -r ~/Desktop/ticktick-home-assistant/custom_components/ticktick /home/zyh/homeassistant/custom_components/
docker restart homeassistant
```

---

### 问题 2：OAuth 仍然报错 "Invalid redirect"

**原因**：TickTick Developer 的 OAuth redirect URL 配置不对

**解决**：
1. 打开 https://developer.ticktick.com/manage
2. 编辑应用
3. **确认** OAuth redirect URL **必须是**：
   ```
   http://192.168.15.194:8123/auth/external/callback
   ```
4. 保存后重新尝试

---

### 问题 3：Authorization 后跳转失败

**原因**：浏览器无法访问 `192.168.15.194:8123`

**解决**：
1. 确认主人的浏览器和 HA 在同一网段（都在 192.168.15.x）
2. 测试：在浏览器直接访问 `http://192.168.15.194:8123`
3. 如果无法访问，检查防火墙或网络配置

---

## 📝 配置文件说明

浮浮酱已经帮主人准备好了完整的配置文件，在 OAuth 认证成功后，浮浮酱会帮主人应用这些配置喵～

配置包括：
1. ✅ REST Command：推送任务列表到 Focus Dial
2. ✅ Input Text Helper：存储今日累计统计
3. ✅ Script：组装 TickTick 任务并推送
4. ✅ Automation：webhook 接收事件 + 累计统计 + 回写 TickTick
5. ✅ Python Script：累计今日秒数

---

## 💡 温馨提示

- **不要着急**：OAuth 配置有时候需要尝试几次喵～
- **仔细检查**：Client ID、Secret、Redirect URL 必须完全一致
- **清理缓存**：如果多次失败，可以尝试清理浏览器缓存或使用无痕模式
- **随时询问**：有任何问题都可以告诉浮浮酱喵～ ฅ'ω'ฅ

---

## 🎯 主人现在要做的

1. **打开浏览器**
2. **访问** `http://192.168.15.194:8123`
3. **按照 Step 1 → Step 2 → Step 3 操作**
4. **完成后告诉浮浮酱结果**

浮浮酱在这里等主人的好消息喵～ φ(≧ω≦*)♪

---

**祝主人配置顺利喵～** o(*￣︶￣*)o

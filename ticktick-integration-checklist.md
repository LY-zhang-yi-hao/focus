# TickTick 集成验证清单

## 目标
验证 Hantick/ticktick-home-assistant 是否满足 Focus Dial × TickTick × HA 方案的需求。

---

## 核心需求（必须满足）

### R1：拉取任务列表
- [ ] 能否提供一个 sensor 实体展示所有 TickTick 任务？
- [ ] sensor attributes 包含哪些字段？
  - [ ] `id`（任务唯一 ID）
  - [ ] `title`（任务标题）
  - [ ] `tags`（标签，用于筛选 `#focus`）
  - [ ] `duration`（建议时长，可能是自定义字段）
- [ ] 能否按标签过滤任务（例如 `#focus`）？

### R2：标记任务完成（M3 核心需求）
- [ ] 是否提供 `ticktick.complete_task` 或类似服务？
- [ ] 服务需要哪些参数？
  - [ ] `task_id`
  - [ ] 其他参数？
- [ ] 调用后 TickTick 是否立即更新？

### R3：配置要求
- [ ] 需要 TickTick 账号密码 or OAuth token？
- [ ] 是否支持 TickTick 官方 Open API？
- [ ] 配置流程是否复杂？

---

## 如何验证（不安装集成的情况下）

### 方法 1：查看 GitHub README（推荐）
1. 打开 https://github.com/Hantick/ticktick-home-assistant
2. 查看 README.md 文件
3. 重点查找：
   - "Services" 章节（是否有 complete_task / mark_done / finish_task 等服务）
   - "Sensors" 章节（sensor 实体提供哪些 attributes）
   - "Configuration" 章节（配置要求）

### 方法 2：查看代码（如果 README 不够详细）
1. 查看 `custom_components/ticktick/` 目录
2. 查看 `services.yaml` 文件（定义了哪些服务）
3. 查看 `sensor.py` 文件（sensor 实体的数据结构）

### 方法 3：查看 Issues/Discussions（了解已知问题）
1. 打开 Issues 标签页
2. 搜索关键词："complete task", "mark done", "#focus", "tags"
3. 看看是否有人遇到类似需求

---

## 决策矩阵

### 场景 A：Hantick 集成满足 R1 + R2
- ✅ **直接使用 Hantick 集成**
- 按照 ROADMAP M1/M2/M3 实施
- 风险：第三方集成可能失效（但可接受）

### 场景 B：Hantick 集成满足 R1，但不满足 R2（无 complete_task 服务）
- ⚠️ **M1/M2 可用，M3 需要备用方案**
- 备用方案：
  - 方案 B1：直接调用 TickTick Open API（REST command）
  - 方案 B2：写一个小服务（Python/Node.js）中转 API 调用
  - 方案 B3：用 Node-RED 处理（如果主人熟悉）

### 场景 C：Hantick 集成不满足 R1（连任务列表都拿不到）
- ❌ **换集成或自己写**
- 备用方案：
  - 方案 C1：用 BeryJu/hass-ticktick（只能创建任务，无法标记完成）
  - 方案 C2：直接用 TickTick Open API（REST sensor + REST command）
  - 方案 C3：写一个最小可用集成（工作量较大）

---

## 浮浮酱的推荐（按优先级）

### Priority 0：先验证再安装（10 分钟）
1. 打开 https://github.com/Hantick/ticktick-home-assistant
2. 查看 README.md 和 services.yaml
3. 确认是否满足 R1 + R2
4. 如果满足 → 继续 Priority 1
5. 如果不满足 → 跳到 Priority 2

### Priority 1：安装并测试 Hantick 集成（30 分钟）
1. 通过 HACS 安装 Hantick/ticktick-home-assistant
2. 配置 TickTick 账号
3. 验证 sensor 实体和 services
4. 测试能否拉取任务列表和标记完成
5. 如果成功 → 继续 ROADMAP M1/M2/M3

### Priority 2：备用方案（TickTick Open API）
如果 Hantick 集成不满足需求，浮浮酱可以帮主人直接对接 TickTick Open API：
- 用 REST sensor 拉取任务列表
- 用 REST command 标记任务完成
- 工作量：约 1-2 小时（配置 + 测试）

---

## 下一步行动

主人请先：
1. 打开 https://github.com/Hantick/ticktick-home-assistant
2. 查看 README.md 或代码
3. 告诉浮浮酱验证结果：
   - [ ] 满足 R1 + R2（完美！）
   - [ ] 满足 R1，不满足 R2（可用，M3 需要备用方案）
   - [ ] 不满足 R1（换方案）

浮浮酱会根据主人的反馈继续推进喵～ ฅ'ω'ฅ

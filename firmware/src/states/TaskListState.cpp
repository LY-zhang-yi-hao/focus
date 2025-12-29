#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskListState.h"

TaskListState::TaskListState()
    : showingCompleted(false),
      selectedIndexPending(0),
      displayOffsetPending(0),
      selectedIndexCompleted(0),
      displayOffsetCompleted(0),
      lastActivity(0)
{
}

void TaskListState::enter()
{
    Serial.println("Entering TaskList State / 进入任务列表状态");

    showingCompleted = pendingTasks.empty() && !completedTasks.empty();
    selectedIndexPending = 0;
    displayOffsetPending = 0;
    selectedIndexCompleted = 0;
    displayOffsetCompleted = 0;
    lastActivity = millis();

    // LED: Cyan breathing to indicate selection mode / 青色呼吸灯表示选择模式
    ledController.setBreath(TEAL, -1, false, 5);

    // Register encoder rotation handler for scrolling / 旋钮控制滚动
    inputController.onEncoderRotateHandler([this](int delta) {
        lastActivity = millis();

        std::vector<FocusTask>& currentTasks = showingCompleted ? completedTasks : pendingTasks;
        int& selectedIndex = showingCompleted ? selectedIndexCompleted : selectedIndexPending;
        int& displayOffset = showingCompleted ? displayOffsetCompleted : displayOffsetPending;

        if (currentTasks.empty()) {
            return;
        }

        if (delta > 0) {
            // Scroll down / 向下滚动
            if (selectedIndex < (int)currentTasks.size() - 1) {
                selectedIndex++;
                // Adjust display offset if needed / 调整显示偏移
                if (selectedIndex - displayOffset >= MAX_VISIBLE_TASKS) {
                    displayOffset++;
                }
            }
        } else if (delta < 0) {
            // Scroll up / 向上滚动
            if (selectedIndex > 0) {
                selectedIndex--;
                // Adjust display offset if needed / 调整显示偏移
                if (selectedIndex < displayOffset) {
                    displayOffset--;
                }
            }
        }
    });

    // Register button press handler for selection / 按键确认选择
    inputController.onPressHandler([this]() {
        lastActivity = millis();

        // 已完成列表：单击切回待办（只读查看）
        if (showingCompleted) {
            showingCompleted = false;
            return;
        }

        if (pendingTasks.empty()) {
            Serial.println("TaskList: No pending tasks, returning to idle / 无待办任务，返回空闲");
            stateMachine.changeState(&StateMachine::idleState);
            return;
        }

        FocusTask* selectedTask = getSelectedTask();
        if (selectedTask == nullptr) {
            stateMachine.changeState(&StateMachine::idleState);
            return;
        }

        Serial.printf("TaskList: Selected task '%s', entering duration select / 选中任务 '%s'，进入时长选择\n",
                      selectedTask->name.c_str(), selectedTask->name.c_str());

        // 进入时长选择状态（而非直接开始计时）/ Enter duration select state
        StateMachine::durationSelectState.setTask(*selectedTask);
        stateMachine.changeState(&StateMachine::durationSelectState);
    });

    // Double press to toggle list type / 双击切换“待办/已完成”
    inputController.onDoublePressHandler([this]() {
        lastActivity = millis();
        showingCompleted = !showingCompleted;
        Serial.printf("TaskList: Toggle list -> %s / 切换列表：%s\n",
                      showingCompleted ? "COMPLETED" : "PENDING",
                      showingCompleted ? "已完成" : "待办");
    });

    // Long press to cancel / 长按取消
    inputController.onLongPressHandler([this]() {
        Serial.println("TaskList: Long press detected, returning to idle / 长按检测，返回空闲");
        displayController.showCancel();
        stateMachine.changeState(&StateMachine::idleState);
    });
}

void TaskListState::update()
{
    inputController.update();
    ledController.update();
    networkController.update();

    // Draw task list on screen / 在屏幕上绘制任务列表
    const std::vector<FocusTask>& currentTasks = showingCompleted ? completedTasks : pendingTasks;
    int currentSelectedIndex = showingCompleted ? selectedIndexCompleted : selectedIndexPending;
    int currentDisplayOffset = showingCompleted ? displayOffsetCompleted : displayOffsetPending;
    displayController.drawTaskListScreen(currentTasks, currentSelectedIndex, currentDisplayOffset, showingCompleted);

    // Check timeout / 检查超时
    if (millis() - lastActivity >= (TASK_TIMEOUT * 1000)) {
        Serial.println("TaskList: Timeout, returning to idle / 超时，返回空闲");
        stateMachine.changeState(&StateMachine::idleState);
    }
}

void TaskListState::exit()
{
    Serial.println("Exiting TaskList State / 离开任务列表状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}

void TaskListState::updateTaskList(const String& jsonTaskList)
{
    Serial.println("TaskList: Updating task list from JSON / 从 JSON 更新任务列表");

    pendingTasks.clear();
    completedTasks.clear();

    // 与 API 端校验保持一致，避免任务较多时解析失败 / Keep in sync with API validation size
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, jsonTaskList);

    if (error) {
        Serial.printf("TaskList: JSON parse error: %s / JSON 解析错误\n", error.c_str());
        return;
    }

    JsonArray tasksArray = doc["tasks"].as<JsonArray>();
    for (JsonObject taskObj : tasksArray) {
        FocusTask task;
        task.id = taskObj["id"].as<String>();
        task.name = taskObj["name"].as<String>();
        task.displayName = taskObj["display_name"] | ""; // 可选：用于设备显示 / Optional device display name
        const String status = taskObj["status"] | "needs_action";
        task.isCompleted = (status == "completed");
        task.estimatedDuration = taskObj["duration"] | 25; // Default 25 min / 默认 25 分钟
        task.spentTodaySeconds = taskObj["spent_today_sec"] | 0;
        task.completedAt = taskObj["completed_at"] | "";
        task.completedSpentSeconds = taskObj["completed_spent_sec"] | 0;

        if (task.isCompleted) {
            completedTasks.push_back(task);
        } else {
            pendingTasks.push_back(task);
        }
    }

    Serial.printf("TaskList: Loaded pending=%d completed=%d / 待办=%d 已完成=%d\n",
                  (int)pendingTasks.size(),
                  (int)completedTasks.size(),
                  (int)pendingTasks.size(),
                  (int)completedTasks.size());

    // Reset selection / 重置选择
    showingCompleted = pendingTasks.empty() && !completedTasks.empty();
    selectedIndexPending = 0;
    displayOffsetPending = 0;
    selectedIndexCompleted = 0;
    displayOffsetCompleted = 0;
    lastActivity = millis();
}

FocusTask* TaskListState::getSelectedTask()
{
    if (showingCompleted) {
        if (selectedIndexCompleted >= 0 && selectedIndexCompleted < (int)completedTasks.size()) {
            return &completedTasks[selectedIndexCompleted];
        }
        return nullptr;
    }

    if (selectedIndexPending >= 0 && selectedIndexPending < (int)pendingTasks.size()) {
        return &pendingTasks[selectedIndexPending];
    }
    return nullptr;
}

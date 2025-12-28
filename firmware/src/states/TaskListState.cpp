#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskListState.h"

TaskListState::TaskListState()
    : selectedIndex(0), displayOffset(0), lastActivity(0)
{
}

void TaskListState::enter()
{
    Serial.println("Entering TaskList State / 进入任务列表状态");

    selectedIndex = 0;
    displayOffset = 0;
    lastActivity = millis();

    // LED: Cyan breathing to indicate selection mode / 青色呼吸灯表示选择模式
    ledController.setBreath(TEAL, -1, false, 5);

    // Register encoder rotation handler for scrolling / 旋钮控制滚动
    inputController.onEncoderRotateHandler([this](int delta) {
        lastActivity = millis();

        if (tasks.empty()) {
            return;
        }

        if (delta > 0) {
            // Scroll down / 向下滚动
            if (selectedIndex < (int)tasks.size() - 1) {
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

        if (tasks.empty()) {
            Serial.println("TaskList: No tasks available, returning to idle / 无任务，返回空闲");
            stateMachine.changeState(&StateMachine::idleState);
            return;
        }

        FocusTask* selectedTask = getSelectedTask();
        if (selectedTask == nullptr) {
            stateMachine.changeState(&StateMachine::idleState);
            return;
        }

        Serial.printf("TaskList: Selected task '%s' (%d min) / 选中任务 '%s' (%d 分钟)\n",
                      selectedTask->name.c_str(), selectedTask->estimatedDuration,
                      selectedTask->name.c_str(), selectedTask->estimatedDuration);

        // Set timer and start / 设置定时器并启动
        StateMachine::timerState.setTimer(
            selectedTask->estimatedDuration,
            0,
            selectedTask->id,
            selectedTask->name,
            "",
            selectedTask->displayName);

        displayController.showTimerStart();
        stateMachine.changeState(&StateMachine::timerState);
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
    displayController.drawTaskListScreen(tasks, selectedIndex, displayOffset);

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

    tasks.clear();

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
        task.estimatedDuration = taskObj["duration"] | 25; // Default 25 min / 默认 25 分钟
        task.spentTodaySeconds = taskObj["spent_today_sec"] | 0;

        tasks.push_back(task);
    }

    Serial.printf("TaskList: Loaded %d tasks / 加载了 %d 个任务\n", (int)tasks.size(), (int)tasks.size());

    // Reset selection / 重置选择
    selectedIndex = 0;
    displayOffset = 0;
    lastActivity = millis();
}

FocusTask* TaskListState::getSelectedTask()
{
    if (selectedIndex >= 0 && selectedIndex < (int)tasks.size()) {
        return &tasks[selectedIndex];
    }
    return nullptr;
}

#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskDetailState.h"
#include <ArduinoJson.h>

TaskDetailState::TaskDetailState()
    : selectedIndex(0),
      displayOffset(0),
      lastActivity(0)
{
}

void TaskDetailState::setTask(const FocusTask& task, const String& projectName)
{
    this->task = task;
    this->projectName = projectName;
}

void TaskDetailState::enter()
{
    Serial.println("Entering TaskDetail State / 进入任务详情状态");

    selectedIndex = 0;
    displayOffset = 0;
    lastActivity = millis();

    // LED：青色呼吸灯，表示详情/勾选模式
    ledController.setBreath(TEAL, -1, false, 3);

    auto totalRows = [this]() -> int {
        // +1：最后一行“完成任务”
        int total = (int)this->task.subtasks.size() + 1;
        if (total <= 0) total = 1;
        return total;
    };

    auto clampSelection = [this, &totalRows]() {
        int total = totalRows();
        if (selectedIndex < 0) selectedIndex = 0;
        if (selectedIndex >= total) selectedIndex = total - 1;
        if (displayOffset < 0) displayOffset = 0;
        int maxOffset = total - MAX_VISIBLE;
        if (maxOffset < 0) maxOffset = 0;
        if (displayOffset > maxOffset) displayOffset = maxOffset;
    };

    inputController.onEncoderRotateHandler([this, &totalRows, &clampSelection](int delta) {
        lastActivity = millis();
        if (delta == 0) return;

        int total = totalRows();
        if (total <= 0) return;

        if (delta > 0) {
            if (selectedIndex < total - 1) {
                selectedIndex++;
                if (selectedIndex - displayOffset >= MAX_VISIBLE) {
                    displayOffset++;
                }
            }
        } else {
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < displayOffset) {
                    displayOffset--;
                }
            }
        }

        clampSelection();
    });

    inputController.onPressHandler([this, &totalRows, &clampSelection]() {
        lastActivity = millis();
        clampSelection();

        const int total = totalRows();
        if (total <= 0) return;

        // 最后一行：完成任务
        if (selectedIndex >= (int)task.subtasks.size()) {
            Serial.println("TaskDetail: Complete task / 详情：完成任务");

            DynamicJsonDocument doc(256);
            doc["event"] = "task_complete";
            doc["project_id"] = task.projectId;
            doc["task_id"] = task.id;
            doc["task_name"] = task.name;

            String payload;
            serializeJson(doc, payload);
            networkController.sendWebhookPayload(payload);

            displayController.showConfirmation();
            stateMachine.changeState(&StateMachine::taskListState);
            return;
        }

        // 子任务：勾选/取消勾选
        if (task.subtasks.empty()) {
            return;
        }

        FocusSubtask& sub = task.subtasks[selectedIndex];
        const bool newCompleted = !sub.isCompleted;
        sub.isCompleted = newCompleted;

        // 重新计算 done/total（仅用于本地即时显示；最终以 HA 推送为准）
        int done = 0;
        for (const auto& it : task.subtasks) {
            if (it.isCompleted) done++;
        }
        task.subtasksDone = done;
        if (task.subtasksTotal <= 0) {
            task.subtasksTotal = (int)task.subtasks.size();
        }

        Serial.printf("TaskDetail: Toggle subtask %d -> %d\n", selectedIndex, (int)newCompleted);

        DynamicJsonDocument doc(256);
        doc["event"] = "subtask_toggle";
        doc["project_id"] = task.projectId;
        doc["task_id"] = task.id;
        doc["task_name"] = task.name;
        doc["item_id"] = sub.id;
        doc["completed"] = newCompleted;

        String payload;
        serializeJson(doc, payload);
        networkController.sendWebhookPayload(payload);
    });

    auto goBack = []() {
        stateMachine.changeState(&StateMachine::durationSelectState);
    };

    inputController.onDoublePressHandler([this, goBack]() {
        lastActivity = millis();
        Serial.println("TaskDetail: Double press - back / 详情：双击返回");
        goBack();
    });

    inputController.onLongPressHandler([this, goBack]() {
        lastActivity = millis();
        Serial.println("TaskDetail: Long press - back / 详情：长按取消返回");
        displayController.showCancel();
        goBack();
    });
}

void TaskDetailState::update()
{
    inputController.update();
    ledController.update();
    networkController.update();

    displayController.drawTaskDetailScreen(projectName, task, selectedIndex, displayOffset);

    if (millis() - lastActivity >= (TIMEOUT_SECONDS * 1000UL)) {
        Serial.println("TaskDetail: Timeout, back to duration select / 详情：超时返回时长选择");
        stateMachine.changeState(&StateMachine::durationSelectState);
    }
}

void TaskDetailState::exit()
{
    Serial.println("Exiting TaskDetail State / 离开任务详情状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}


#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskListState.h"
#include <ArduinoJson.h>

TaskListState::TaskListState()
    : selectedProjectId(""),
      selectedProjectName(""),
      mode(TaskListMode::Pending),
      selectedIndexPending(0),
      displayOffsetPending(0),
      selectedIndexCompleted(0),
      displayOffsetCompleted(0),
      selectedIndexProjects(0),
      displayOffsetProjects(0),
      lastActivity(0)
{
}

void TaskListState::enter()
{
    Serial.println("Entering TaskList State / 进入任务列表状态");

    mode = pendingTasks.empty() && !completedTasks.empty() ? TaskListMode::Completed : TaskListMode::Pending;
    selectedIndexPending = 0;
    displayOffsetPending = 0;
    selectedIndexCompleted = 0;
    displayOffsetCompleted = 0;
    selectedIndexProjects = 0;
    displayOffsetProjects = 0;
    lastActivity = millis();

    // LED: Cyan breathing to indicate selection mode / 青色呼吸灯表示选择模式
    ledController.setBreath(TEAL, -1, false, 5);

    // Register encoder rotation handler for scrolling / 旋钮控制滚动
    inputController.onEncoderRotateHandler([this](int delta) {
        lastActivity = millis();

        if (delta == 0) {
            return;
        }

        if (mode == TaskListMode::Projects) {
            if (projects.empty()) {
                return;
            }
            int& selectedIndex = selectedIndexProjects;
            int& displayOffset = displayOffsetProjects;

            if (delta > 0) {
                if (selectedIndex < (int)projects.size() - 1) {
                    selectedIndex++;
                    if (selectedIndex - displayOffset >= MAX_VISIBLE_TASKS) {
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
            return;
        }

        std::vector<FocusTask>& currentTasks = (mode == TaskListMode::Completed) ? completedTasks : pendingTasks;
        int& selectedIndex = (mode == TaskListMode::Completed) ? selectedIndexCompleted : selectedIndexPending;
        int& displayOffset = (mode == TaskListMode::Completed) ? displayOffsetCompleted : displayOffsetPending;

        if (currentTasks.empty()) {
            return;
        }

        if (delta > 0) {
            if (selectedIndex < (int)currentTasks.size() - 1) {
                selectedIndex++;
                if (selectedIndex - displayOffset >= MAX_VISIBLE_TASKS) {
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
    });

    // Register button press handler for selection / 按键确认选择
    inputController.onPressHandler([this]() {
        lastActivity = millis();

        // 项目选择：单击选择项目并请求 HA 刷新
        if (mode == TaskListMode::Projects) {
            if (projects.empty()) {
                return;
            }
            if (selectedIndexProjects < 0) selectedIndexProjects = 0;
            if (selectedIndexProjects >= (int)projects.size()) selectedIndexProjects = (int)projects.size() - 1;

            const FocusProject& p = projects[selectedIndexProjects];
            selectedProjectId = p.id;
            selectedProjectName = p.name;

            DynamicJsonDocument doc(256);
            doc["event"] = "project_selected";
            doc["project_id"] = selectedProjectId;
            doc["project_name"] = selectedProjectName;
            String payload;
            serializeJson(doc, payload);
            networkController.sendWebhookPayload(payload);

            // 选择项目后回到“待办”
            mode = TaskListMode::Pending;
            selectedIndexPending = 0;
            displayOffsetPending = 0;
            return;
        }

        // 已完成列表：单击切回待办（只读查看）
        if (mode == TaskListMode::Completed) {
            mode = TaskListMode::Pending;
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

    // Double press to cycle mode / 双击循环：待办 → 已完成 → 项目选择
    inputController.onDoublePressHandler([this]() {
        lastActivity = millis();
        if (mode == TaskListMode::Pending) {
            mode = TaskListMode::Completed;
        } else if (mode == TaskListMode::Completed) {
            mode = TaskListMode::Projects;

            // 进入项目选择时，尽量定位到当前项目
            int idx = 0;
            for (int i = 0; i < (int)projects.size(); i++) {
                if (projects[i].id == selectedProjectId) {
                    idx = i;
                    break;
                }
            }
            selectedIndexProjects = idx;
            displayOffsetProjects = (idx >= MAX_VISIBLE_TASKS) ? (idx - (MAX_VISIBLE_TASKS - 1)) : 0;
        } else {
            mode = TaskListMode::Pending;
        }

        Serial.printf("TaskList: Mode -> %d\n", (int)mode);
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

    if (mode == TaskListMode::Projects) {
        displayController.drawProjectSelectScreen(projects, selectedIndexProjects, displayOffsetProjects, selectedProjectId, false);
    } else {
        const bool showingCompleted = (mode == TaskListMode::Completed);
        const std::vector<FocusTask>& currentTasks = showingCompleted ? completedTasks : pendingTasks;
        int currentSelectedIndex = showingCompleted ? selectedIndexCompleted : selectedIndexPending;
        int currentDisplayOffset = showingCompleted ? displayOffsetCompleted : displayOffsetPending;
        displayController.drawTaskListScreen(selectedProjectName, currentTasks, currentSelectedIndex, currentDisplayOffset, showingCompleted);
    }

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
    projects.clear();

    // 与 API 端校验保持一致，避免任务较多时解析失败 / Keep in sync with API validation size
    DynamicJsonDocument doc(24576);
    DeserializationError error = deserializeJson(doc, jsonTaskList);

    if (error) {
        Serial.printf("TaskList: JSON parse error: %s / JSON 解析错误\n", error.c_str());
        return;
    }

    selectedProjectId = doc["selected_project_id"] | "";
    selectedProjectName = doc["selected_project_name"] | "";

    if (doc["projects"].is<JsonArray>()) {
        for (JsonObject p : doc["projects"].as<JsonArray>()) {
            FocusProject proj;
            proj.id = p["id"] | "";
            proj.name = p["name"] | "";
            if (!proj.id.isEmpty() && !proj.name.isEmpty()) {
                projects.push_back(proj);
            }
        }

        if (selectedProjectName.isEmpty() && !selectedProjectId.isEmpty()) {
            for (const auto& p : projects) {
                if (p.id == selectedProjectId) {
                    selectedProjectName = p.name;
                    break;
                }
            }
        }
    }

    JsonArray tasksArray = doc["tasks"].as<JsonArray>();
    for (JsonObject taskObj : tasksArray) {
        FocusTask task;
        task.id = taskObj["id"] | "";
        task.projectId = taskObj["project_id"] | taskObj["projectId"] | selectedProjectId;
        task.name = taskObj["name"] | taskObj["title"] | "";
        task.displayName = taskObj["display_name"] | "";
        const String status = taskObj["status"] | "needs_action";
        task.isCompleted = (status == "completed");
        task.estimatedDuration = taskObj["duration"] | 25; // Default 25 min / 默认 25 分钟
        task.spentTodaySeconds = taskObj["spent_today_sec"] | 0;
        task.completedAt = taskObj["completed_mmdd"] | taskObj["completed_at"] | "";
        task.completedSpentSeconds = taskObj["completed_spent_sec"] | 0;

        task.priority = taskObj["priority"] | 0;
        task.priorityFlag = taskObj["priority_flag"] | "";
        task.dueMmdd = taskObj["due_mmdd"] | "";
        task.hasRepeat = taskObj["has_repeat"] | false;
        task.hasReminder = taskObj["has_reminder"] | false;
        task.subtasksTotal = taskObj["subtasks_total"] | 0;
        task.subtasksDone = taskObj["subtasks_done"] | 0;

        task.subtasks.clear();
        if (taskObj["subtasks"].is<JsonArray>()) {
            int done = 0;
            for (JsonObject it : taskObj["subtasks"].as<JsonArray>()) {
                FocusSubtask sub;
                sub.id = it["id"] | "";
                sub.title = it["title"] | "";
                int st = it["status"] | 0;
                sub.isCompleted = (st == 1);
                if (!sub.id.isEmpty() && !sub.title.isEmpty()) {
                    task.subtasks.push_back(sub);
                    if (sub.isCompleted) done++;
                }
            }
            if (task.subtasksTotal <= 0) {
                task.subtasksTotal = (int)task.subtasks.size();
            }
            if (task.subtasksDone <= 0) {
                task.subtasksDone = done;
            }
        }

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
    if (mode != TaskListMode::Projects) {
        mode = pendingTasks.empty() && !completedTasks.empty() ? TaskListMode::Completed : TaskListMode::Pending;
    }
    selectedIndexPending = 0;
    displayOffsetPending = 0;
    selectedIndexCompleted = 0;
    displayOffsetCompleted = 0;
    selectedIndexProjects = 0;
    displayOffsetProjects = 0;
    lastActivity = millis();
}

FocusTask* TaskListState::getSelectedTask()
{
    if (mode == TaskListMode::Completed) {
        if (selectedIndexCompleted >= 0 && selectedIndexCompleted < (int)completedTasks.size()) {
            return &completedTasks[selectedIndexCompleted];
        }
        return nullptr;
    }

    if (mode == TaskListMode::Pending && selectedIndexPending >= 0 && selectedIndexPending < (int)pendingTasks.size()) {
        return &pendingTasks[selectedIndexPending];
    }
    return nullptr;
}

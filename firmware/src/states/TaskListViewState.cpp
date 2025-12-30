#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskListViewState.h"

TaskListViewState::TaskListViewState()
    : timerDuration(0),
      timerElapsedTime(0),
      timerTaskProjectId(""),
      timerTaskId(""),
      timerTaskName(""),
      timerSessionId(""),
      timerTaskDisplayName(""),
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

void TaskListViewState::enter()
{
    Serial.println("Entering TaskListView State (read-only) / 进入任务列表查看状态（只读）");

    // 重置查看状态
    mode = TaskListMode::Pending;
    selectedIndexPending = 0;
    displayOffsetPending = 0;
    selectedIndexCompleted = 0;
    displayOffsetCompleted = 0;
    selectedIndexProjects = 0;
    displayOffsetProjects = 0;
    lastActivity = millis();

    // LED: 青色呼吸灯（与TaskListState一致但更暗，表示只读）
    ledController.setBreath(TEAL, -1, false, 3);

    // 获取任务列表引用
    auto& pendingTasks = StateMachine::taskListState.pendingTasks;
    auto& completedTasks = StateMachine::taskListState.completedTasks;
    auto& projects = StateMachine::taskListState.projects;

    // 旋钮滚动查看 / Encoder scrolls through tasks
    inputController.onEncoderRotateHandler([this, &pendingTasks, &completedTasks, &projects](int delta) {
        lastActivity = millis();

        if (delta == 0) {
            return;
        }

        if (mode == TaskListMode::Projects) {
            if (projects.empty()) return;
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

    // 单击返回计时状态 / Click to return to timer
    inputController.onPressHandler([this]() {
        lastActivity = millis();

        // 项目选择：单击选择项目并请求 HA 刷新
        if (mode == TaskListMode::Projects) {
            auto& projects = StateMachine::taskListState.projects;
            if (projects.empty()) {
                return;
            }
            if (selectedIndexProjects < 0) selectedIndexProjects = 0;
            if (selectedIndexProjects >= (int)projects.size()) selectedIndexProjects = (int)projects.size() - 1;

            const FocusProject& p = projects[selectedIndexProjects];

            DynamicJsonDocument doc(256);
            doc["event"] = "project_selected";
            doc["project_id"] = p.id;
            doc["project_name"] = p.name;
            String payload;
            serializeJson(doc, payload);
            networkController.sendWebhookPayload(payload);

            mode = TaskListMode::Pending;
            selectedIndexPending = 0;
            displayOffsetPending = 0;
            return;
        }

        Serial.println("TaskListView: Returning to timer");

        // 恢复计时器状态（不重新发送webhook）
        StateMachine::timerState.setTimer(
            timerDuration,
            timerElapsedTime,
            timerTaskId,
            timerTaskName,
            timerSessionId,
            timerTaskDisplayName,
            timerTaskProjectId);

        stateMachine.changeState(&StateMachine::timerState);
    });

    // 双击循环：待办 → 已完成 → 项目选择
    inputController.onDoublePressHandler([this]() {
        lastActivity = millis();
        if (mode == TaskListMode::Pending) {
            mode = TaskListMode::Completed;
        } else if (mode == TaskListMode::Completed) {
            mode = TaskListMode::Projects;

            // 进入项目选择时，尽量定位到当前项目
            const String currentId = StateMachine::taskListState.selectedProjectId;
            int idx = 0;
            for (int i = 0; i < (int)StateMachine::taskListState.projects.size(); i++) {
                if (StateMachine::taskListState.projects[i].id == currentId) {
                    idx = i;
                    break;
                }
            }
            selectedIndexProjects = idx;
            displayOffsetProjects = (idx >= MAX_VISIBLE_TASKS) ? (idx - (MAX_VISIBLE_TASKS - 1)) : 0;
        } else {
            mode = TaskListMode::Pending;
        }

        Serial.printf("TaskListView: Mode -> %d\n", (int)mode);
    });
}

void TaskListViewState::update()
{
    inputController.update();
    ledController.update();
    networkController.update();

    // 获取任务列表引用
    const auto& pendingTasks = StateMachine::taskListState.pendingTasks;
    const auto& completedTasks = StateMachine::taskListState.completedTasks;
    const auto& projects = StateMachine::taskListState.projects;

    if (mode == TaskListMode::Projects) {
        displayController.drawProjectSelectScreen(
            projects,
            selectedIndexProjects,
            displayOffsetProjects,
            StateMachine::taskListState.selectedProjectId,
            true
        );
    } else {
        const bool showingCompleted = (mode == TaskListMode::Completed);
        const std::vector<FocusTask>& currentTasks = showingCompleted ? completedTasks : pendingTasks;
        int currentSelectedIndex = showingCompleted ? selectedIndexCompleted : selectedIndexPending;
        int currentDisplayOffset = showingCompleted ? displayOffsetCompleted : displayOffsetPending;

        displayController.drawTaskListViewScreen(
            StateMachine::taskListState.selectedProjectName,
            currentTasks,
            currentSelectedIndex,
            currentDisplayOffset,
            showingCompleted
        );
    }

    // 超时返回计时状态 / Timeout returns to timer
    if (millis() - lastActivity >= (VIEW_TIMEOUT * 1000)) {
        Serial.println("TaskListView: Timeout, returning to timer");

        StateMachine::timerState.setTimer(
            timerDuration,
            timerElapsedTime,
            timerTaskId,
            timerTaskName,
            timerSessionId,
            timerTaskDisplayName,
            timerTaskProjectId);

        stateMachine.changeState(&StateMachine::timerState);
    }
}

void TaskListViewState::exit()
{
    Serial.println("Exiting TaskListView State / 离开任务列表查看状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}

void TaskListViewState::setTimerContext(int duration, unsigned long elapsedTime,
                                        const String& taskId, const String& taskName,
                                        const String& sessionId, const String& taskDisplayName,
                                        const String& taskProjectId)
{
    timerDuration = duration;
    timerElapsedTime = elapsedTime;
    timerTaskProjectId = taskProjectId;
    timerTaskId = taskId;
    timerTaskName = taskName;
    timerSessionId = sessionId;
    timerTaskDisplayName = taskDisplayName;
}

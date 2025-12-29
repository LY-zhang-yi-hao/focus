#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskListViewState.h"

TaskListViewState::TaskListViewState()
    : timerDuration(0),
      timerElapsedTime(0),
      showingCompleted(false),
      selectedIndexPending(0),
      displayOffsetPending(0),
      selectedIndexCompleted(0),
      displayOffsetCompleted(0),
      lastActivity(0)
{
}

void TaskListViewState::enter()
{
    Serial.println("Entering TaskListView State (read-only) / 进入任务列表查看状态（只读）");

    // 重置查看状态
    showingCompleted = false;
    selectedIndexPending = 0;
    displayOffsetPending = 0;
    selectedIndexCompleted = 0;
    displayOffsetCompleted = 0;
    lastActivity = millis();

    // LED: 青色呼吸灯（与TaskListState一致但更暗，表示只读）
    ledController.setBreath(TEAL, -1, false, 3);

    // 获取任务列表引用
    auto& pendingTasks = StateMachine::taskListState.pendingTasks;
    auto& completedTasks = StateMachine::taskListState.completedTasks;

    // 旋钮滚动查看 / Encoder scrolls through tasks
    inputController.onEncoderRotateHandler([this, &pendingTasks, &completedTasks](int delta) {
        lastActivity = millis();

        std::vector<FocusTask>& currentTasks = showingCompleted ? completedTasks : pendingTasks;
        int& selectedIndex = showingCompleted ? selectedIndexCompleted : selectedIndexPending;
        int& displayOffset = showingCompleted ? displayOffsetCompleted : displayOffsetPending;

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
        } else if (delta < 0) {
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
        Serial.println("TaskListView: Returning to timer");

        // 恢复计时器状态（不重新发送webhook）
        StateMachine::timerState.setTimer(
            timerDuration,
            timerElapsedTime,
            timerTaskId,
            timerTaskName,
            timerSessionId,
            timerTaskDisplayName);

        stateMachine.changeState(&StateMachine::timerState);
    });

    // 双击切换待办/已完成 / Double click to toggle list type
    inputController.onDoublePressHandler([this]() {
        lastActivity = millis();
        showingCompleted = !showingCompleted;
        Serial.printf("TaskListView: Toggle list -> %s\n",
                      showingCompleted ? "COMPLETED" : "PENDING");
    });
}

void TaskListViewState::update()
{
    inputController.update();
    ledController.update();

    // 获取任务列表引用
    const auto& pendingTasks = StateMachine::taskListState.pendingTasks;
    const auto& completedTasks = StateMachine::taskListState.completedTasks;

    const std::vector<FocusTask>& currentTasks = showingCompleted ? completedTasks : pendingTasks;
    int currentSelectedIndex = showingCompleted ? selectedIndexCompleted : selectedIndexPending;
    int currentDisplayOffset = showingCompleted ? displayOffsetCompleted : displayOffsetPending;

    // 绘制只读任务列表（带"查看中"标记）
    displayController.drawTaskListViewScreen(currentTasks, currentSelectedIndex, currentDisplayOffset, showingCompleted);

    // 超时返回计时状态 / Timeout returns to timer
    if (millis() - lastActivity >= (VIEW_TIMEOUT * 1000)) {
        Serial.println("TaskListView: Timeout, returning to timer");

        StateMachine::timerState.setTimer(
            timerDuration,
            timerElapsedTime,
            timerTaskId,
            timerTaskName,
            timerSessionId,
            timerTaskDisplayName);

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
                                        const String& sessionId, const String& taskDisplayName)
{
    timerDuration = duration;
    timerElapsedTime = elapsedTime;
    timerTaskId = taskId;
    timerTaskName = taskName;
    timerSessionId = sessionId;
    timerTaskDisplayName = taskDisplayName;
}

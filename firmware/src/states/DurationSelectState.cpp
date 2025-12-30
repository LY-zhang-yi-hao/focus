#include "StateMachine.h"
#include "Controllers.h"
#include "states/DurationSelectState.h"

DurationSelectState::DurationSelectState()
    : duration(DEFAULT_TIMER),
      lastActivity(0)
{
}

void DurationSelectState::enter()
{
    Serial.println("Entering DurationSelect State / 进入时长选择状态");
    Serial.printf("Task: %s, Default duration: %d min\n",
                  selectedTask.name.c_str(), duration);

    lastActivity = millis();

    // LED: 琥珀色呼吸灯表示调节模式 / Amber breathing for adjustment mode
    ledController.setBreath(AMBER, -1, false, 5);

    // 旋钮调节时长 / Encoder adjusts duration
    inputController.onEncoderRotateHandler([this](int delta) {
        lastActivity = millis();

        duration += (delta * DURATION_STEP);

        // 边界检查 / Bounds check
        if (duration < DURATION_MIN) {
            duration = DURATION_MIN;
        } else if (duration > DURATION_MAX) {
            duration = DURATION_MAX;
        }

        Serial.printf("DurationSelect: Adjusted to %d min\n", duration);
    });

    // 单击确认开始计时 / Click to confirm and start timer
    inputController.onPressHandler([this]() {
        Serial.printf("DurationSelect: Confirmed %d min for task '%s'\n",
                      duration, selectedTask.name.c_str());

        // 设置定时器并启动 / Set timer and start
        StateMachine::timerState.setTimer(
            duration,
            0,
            selectedTask.id,
            selectedTask.name,
            "",
            selectedTask.displayName,
            selectedTask.projectId);

        displayController.showTimerStart();
        stateMachine.changeState(&StateMachine::timerState);
    });

    // 双击查看任务详情（子任务）/ Double press to view task detail (subtasks)
    inputController.onDoublePressHandler([this]() {
        Serial.println("DurationSelect: Double press - task detail / 双击进入任务详情");
        StateMachine::taskDetailState.setTask(selectedTask, StateMachine::taskListState.selectedProjectName);
        stateMachine.changeState(&StateMachine::taskDetailState);
    });

    // 长按取消返回任务列表 / Long press to cancel and return to task list
    inputController.onLongPressHandler([]() {
        Serial.println("DurationSelect: Canceled, returning to task list");
        displayController.showCancel();
        stateMachine.changeState(&StateMachine::taskListState);
    });
}

void DurationSelectState::update()
{
    inputController.update();
    ledController.update();

    // 绘制时长选择界面 / Draw duration select screen
    displayController.drawDurationSelectScreen(selectedTask.name, duration);

    // 超时返回任务列表 / Timeout returns to task list
    if (millis() - lastActivity >= (SELECT_TIMEOUT * 1000)) {
        Serial.println("DurationSelect: Timeout, returning to task list");
        stateMachine.changeState(&StateMachine::taskListState);
    }
}

void DurationSelectState::exit()
{
    Serial.println("Exiting DurationSelect State / 离开时长选择状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}

void DurationSelectState::setTask(const FocusTask& task)
{
    selectedTask = task;
    // 使用任务的预设时长作为初始值，如果没有则使用默认值
    duration = (task.estimatedDuration > 0) ? task.estimatedDuration : DEFAULT_TIMER;
}

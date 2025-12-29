#pragma once

#include "State.h"
#include "models/FocusTask.h"
#include <Arduino.h>
#include <vector>

/**
 * 任务列表查看状态：专注中只读查看任务列表
 *
 * 特点：
 * - 只读模式，不能选择任务
 * - 不暂停计时、不触发webhook
 * - 单击或超时返回 TimerState
 *
 * 交互：
 * - 旋钮：滚动查看任务
 * - 单击：返回计时状态
 * - 双击：切换待办/已完成列表
 */
class TaskListViewState : public State {
public:
    TaskListViewState();
    void enter() override;
    void update() override;
    void exit() override;

    // 设置返回时的计时器上下文 / Set timer context for returning
    void setTimerContext(int duration, unsigned long elapsedTime,
                         const String& taskId, const String& taskName,
                         const String& sessionId, const String& taskDisplayName);

private:
    // 计时器上下文（返回时恢复）
    int timerDuration;
    unsigned long timerElapsedTime;
    String timerTaskId;
    String timerTaskName;
    String timerSessionId;
    String timerTaskDisplayName;

    // 查看状态
    bool showingCompleted;
    int selectedIndexPending;
    int displayOffsetPending;
    int selectedIndexCompleted;
    int displayOffsetCompleted;
    unsigned long lastActivity;

    static const int MAX_VISIBLE_TASKS = 2;
    static const int VIEW_TIMEOUT = 15;  // 15秒超时返回
};

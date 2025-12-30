#pragma once

#include "State.h"
#include "models/FocusTask.h"
#include <Arduino.h>

/**
 * 任务详情状态：展示子任务（ChecklistItem）并支持勾选/完成任务
 *
 * 入口：DurationSelectState 双击进入
 * 交互：
 * - 旋钮：滚动选择子任务/“完成任务”
 * - 单击：勾选/取消勾选子任务；或触发“完成任务”
 * - 双击：返回时长选择
 * - 长按：取消返回时长选择
 */
class TaskDetailState : public State {
public:
    TaskDetailState();

    void enter() override;
    void update() override;
    void exit() override;

    // 设置当前查看的任务 / Set current task context
    void setTask(const FocusTask& task, const String& projectName);

private:
    FocusTask task;
    String projectName;

    int selectedIndex;
    int displayOffset;
    unsigned long lastActivity;

    static const int MAX_VISIBLE = 2;
    static const int TIMEOUT_SECONDS = 30;
};


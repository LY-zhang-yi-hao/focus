#pragma once

#include "State.h"
#include "models/FocusTask.h"
#include <Arduino.h>

/**
 * 时长选择状态：选择任务后，允许用户调节专注时长
 *
 * 流程：TaskListState 选择任务 → DurationSelectState 调节时长 → TimerState 开始计时
 *
 * 交互：
 * - 旋钮：调节时长（步进5分钟，范围5-120分钟）
 * - 单击：确认并开始计时
 * - 长按：取消返回任务列表
 */
class DurationSelectState : public State {
public:
    DurationSelectState();
    void enter() override;
    void update() override;
    void exit() override;

    // 设置选中的任务信息 / Set selected task info
    void setTask(const FocusTask& task);

private:
    FocusTask selectedTask;      // 选中的任务
    int duration;                // 当前选择的时长（分钟）
    unsigned long lastActivity;  // 最后操作时间

    static const int DURATION_STEP = 5;      // 步进5分钟
    static const int DURATION_MIN = 5;       // 最小5分钟
    static const int DURATION_MAX = 120;     // 最大120分钟
    static const int SELECT_TIMEOUT = 30;    // 超时30秒
};

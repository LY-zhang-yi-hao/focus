#pragma once

#include "State.h"
#include <Arduino.h>

// TaskCompletePromptState / 任务结束确认状态
//
// 用于在“计时结束”或“中途取消”后，让用户选择是否将该任务标记为 TickTick 已完成。
class TaskCompletePromptState : public State
{
public:
    TaskCompletePromptState();

    void enter() override;
    void update() override;
    void exit() override;

    // Set context before entering this state / 进入前设置上下文
    void setContext(const String& taskId,
                    const String& taskName,
                    const String& sessionId,
                    uint32_t elapsedSeconds,
                    bool countTime,
                    bool isCanceled,
                    const String& taskDisplayName = "",
                    const String& taskProjectId = "");

private:
    String taskId;
    String taskName;
    String sessionId;
    String taskDisplayName;
    String taskProjectId;
    uint32_t elapsedSeconds;
    bool countTime;
    bool isCanceled;
    bool markDoneSelected;
};

#pragma once

#include <Arduino.h>
#include "State.h"
#include "states/AdjustState.h"
#include "states/DoneState.h"
#include "states/IdleState.h"
#include "states/PausedState.h"
#include "states/ProvisionState.h"
#include "states/ResetState.h"
#include "states/SleepState.h"
#include "states/StartupState.h"
#include "states/TaskCompletePromptState.h"
#include "states/TaskListState.h"
#include "states/TimerState.h"

class StateMachine {
public:
    StateMachine();
    ~StateMachine();

    void changeState(State* newState);
    void update();
    State* getCurrentState();

    // Static states / 静态状态对象
    static AdjustState adjustState;
    static SleepState sleepState;
    static DoneState doneState;
    static IdleState idleState;
    static PausedState pausedState;
    static ProvisionState provisionState;
    static ResetState resetState;
    static StartupState startupState;
    static TaskCompletePromptState taskCompletePromptState;
    static TaskListState taskListState;
    static TimerState timerState;

private:
    State* currentState;            // Pointer to the current state / 当前状态指针
    SemaphoreHandle_t stateMutex;   // Mutex to protect transitions / 保护状态切换的互斥量
    bool transition = false;
};

extern StateMachine stateMachine;  // Global instance of the StateMachine / 状态机全局实例

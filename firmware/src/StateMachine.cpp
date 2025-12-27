#include "StateMachine.h"

// Global state machine instance / 状态机全局实例
StateMachine stateMachine;

// Initialize static states / 初始化静态状态实例
AdjustState StateMachine::adjustState;
SleepState StateMachine::sleepState;
DoneState StateMachine::doneState;
IdleState StateMachine::idleState;
PausedState StateMachine::pausedState;
ProvisionState StateMachine::provisionState;
ResetState StateMachine::resetState;
StartupState StateMachine::startupState;
TimerState StateMachine::timerState;

StateMachine::StateMachine() {
    currentState = &startupState;  // Start with StartupState / 初始状态为启动态
    stateMutex = xSemaphoreCreateMutex();  // Initialize the mutex / 创建互斥量
}

// Clean up the state and delete the mutex / 清理状态并释放互斥量
StateMachine::~StateMachine() {
    if (stateMutex != NULL) {
        vSemaphoreDelete(stateMutex);  // Delete the mutex / 删除互斥量
    }
}

void StateMachine::changeState(State* newState) {
    // Lock the mutex / 锁定互斥量
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        transition = true;
        if (currentState != nullptr) {
            currentState->exit();
        }
        currentState = newState;  // Assign the new state (static state) / 切换到新的静态状态
        currentState->enter();
        transition = false;
        xSemaphoreGive(stateMutex);  // Release the mutex / 释放互斥量
    }
}

void StateMachine::update() {
    if (!transition && currentState != nullptr) {
        currentState->update();  // Call update on the current state / 调用当前状态的 update
    }
}

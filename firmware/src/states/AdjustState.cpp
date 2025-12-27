#include "StateMachine.h"
#include "Controllers.h"

void AdjustState::enter()
{
    Serial.println("Entering Adjust State / 进入调整状态");

    lastActivity = millis();
    ledController.setSolid(AMBER);

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([this]()
                                   {
        Serial.println("Adjust State: Button pressed / 调整状态：按键按下"); 
        
        StateMachine::idleState.setTimer(this->adjustDuration);
        displayController.showConfirmation();
        stateMachine.changeState(&StateMachine::idleState); });

    inputController.onEncoderRotateHandler([this](int delta)
                                           {
        Serial.println("Adjust State: Encoder turned / 调整状态：旋钮转动");
        Serial.println(delta); // 输出增量
        
        // Update duration with delta and enforce bounds / 根据增量更新时间并做边界校验
        this->adjustDuration += (delta * 5);
        if (this->adjustDuration < MIN_TIMER) {
            this->adjustDuration = MIN_TIMER;
        } else if (this->adjustDuration > MAX_TIMER) {
            this->adjustDuration = MAX_TIMER;
        }

        this->lastActivity = millis(); });
}

void AdjustState::update()
{
    inputController.update();
    displayController.drawAdjustScreen(adjustDuration);

    if (millis() - lastActivity >= (CHANGE_TIMEOUT * 1000))
    {
        // Transition to Idle / 超时返回空闲
        stateMachine.changeState(&StateMachine::idleState);
    }
}

void AdjustState::exit()
{
    Serial.println("Exiting Adjust State / 离开调整状态");
    inputController.releaseHandlers();
}

void AdjustState::adjustTimer(int duration)
{
    adjustDuration = duration;
}

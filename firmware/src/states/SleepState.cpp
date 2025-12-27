#include "StateMachine.h"
#include "Controllers.h"

void SleepState::enter()
{
    Serial.println("Entering Sleep State / 进入休眠状态");

    ledController.turnOff();
    displayController.clear();

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([]()
                                   {
        Serial.println("Sleep State: Button pressed / 休眠状态：按键按下");
        stateMachine.changeState(&StateMachine::idleState); });

    inputController.onLongPressHandler([]()
                                       {
        Serial.println("Sleep State: long pressed / 休眠状态：长按");
        stateMachine.changeState(&StateMachine::idleState); });

    inputController.onEncoderRotateHandler([this](int delta)
                                           {
        Serial.println("Sleep State: Encoder turned / 休眠状态：旋钮转动");
        stateMachine.changeState(&StateMachine::idleState); });
}

void SleepState::update()
{
    inputController.update();
}

void SleepState::exit()
{
    Serial.println("Exiting Sleep State / 离开休眠状态");
    inputController.releaseHandlers();
}

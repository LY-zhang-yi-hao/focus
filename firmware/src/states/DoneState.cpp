#include "StateMachine.h"
#include "Controllers.h"

DoneState::DoneState() : doneEnter(0) {}

void DoneState::enter()
{
    Serial.println("Entering Done State / 进入完成状态");

    doneEnter = millis();
    ledController.setBreath(GREEN, -1, true, 2);

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([]()
                                   {
        Serial.println("Done State: Button pressed / 完成状态：按键按下");
        stateMachine.changeState(&StateMachine::idleState); });
}

void DoneState::update()
{
    inputController.update();
    ledController.update();

    displayController.drawDoneScreen();

    if (millis() - doneEnter >= (CHANGE_TIMEOUT * 1000))
    {
        // Transition to Idle after timeout / 超时后回到空闲
        stateMachine.changeState(&StateMachine::idleState);
    }
}

void DoneState::exit()
{
    Serial.println("Exiting Done State / 离开完成状态");
    inputController.releaseHandlers();
}

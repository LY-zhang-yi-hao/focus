#include "StateMachine.h"
#include "Controllers.h"

PausedState::PausedState() : duration(0), elapsedTime(0), pauseEnter(0) {}

void PausedState::enter()
{
    Serial.println("Entering Paused State / 进入暂停状态");
    pauseEnter = millis(); // Record the time when the pause started / 记录暂停起始时间
    ledController.setBreath(YELLOW, -1, false, 20);

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([this]()
                                   {
                                       Serial.println("Paused State: Button Pressed / 暂停状态：按键按下");

                                       // Send 'Start' webhook (resume)
                                       networkController.sendWebhookAction("start");

                                       // Transition back to TimerState with the stored duration and elapsed time
                                       StateMachine::timerState.setTimer(duration, elapsedTime);
                                       displayController.showTimerResume();
                                       stateMachine.changeState(&StateMachine::timerState); // Transition back to Timer State / 返回计时状态
                                   });

    inputController.onDoublePressHandler([]()
                                         {
                                             Serial.println("Paused State: Button Double Pressed / 暂停状态：双击");

                                             // Send 'Stop' webhook (canceled)
                                             networkController.sendWebhookAction("stop");
                                             displayController.showCancel();
                                             stateMachine.changeState(&StateMachine::idleState); // Transition back to Idle State / 返回空闲状态
                                         });
}

void PausedState::update()
{
    inputController.update();
    ledController.update();

    // Redraw the paused screen with remaining time / 按剩余时间重绘暂停界面
    int remainingTime = (duration * 60) - elapsedTime;
    displayController.drawPausedScreen(remainingTime);

    unsigned long currentTime = millis();

    // Check if the pause timeout has been reached / 检查是否超过暂停超时
    if (currentTime - pauseEnter >= (PAUSE_TIMEOUT * 60 * 1000))
    {
        // Timeout reached, transition to Idle State / 超时后回到空闲
        Serial.println("Paused State: Timout / 暂停状态：超时");

        // Send 'Stop' webhook (timeout)
        networkController.sendWebhookAction("stop");
        displayController.showCancel();
        stateMachine.changeState(&StateMachine::idleState); // Transition back to Idle State / 返回空闲
    }
}

void PausedState::exit()
{
    Serial.println("Exiting Paused State / 离开暂停状态");
    inputController.releaseHandlers();
}

void PausedState::setPause(int duration, unsigned long elapsedTime)
{
    this->duration = duration;
    this->elapsedTime = elapsedTime;
}

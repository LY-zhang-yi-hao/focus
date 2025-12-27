#include "StateMachine.h"
#include "Controllers.h"

TimerState::TimerState() : duration(0), elapsedTime(0), startTime(0) {}

void TimerState::enter()
{
    Serial.println("Entering Timer State / 进入计时状态");

    // Start time based on the elapsed time / 根据已消耗时间校准起点
    startTime = millis() - (elapsedTime * 1000);

    displayController.drawTimerScreen(duration * 60);
    ledController.startFillAndDecay(RED, ((duration * 60) - elapsedTime) * 1000);

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([this]()
                                   {
                                       Serial.println("Timer State: Button Pressed / 计时状态：按键按下");

                                       // Send 'Stop' webhook (pause)
                                       networkController.sendWebhookAction("stop");
                                       displayController.showTimerPause();

                                       // Transition to PausedState and set elapsed time
                                       StateMachine::pausedState.setPause(this->duration, this->elapsedTime); // Save current elapsed time / 保存当前已过时间
                                       stateMachine.changeState(&StateMachine::pausedState);                  // Transition to Paused State / 切换到暂停状态
                                   });

    inputController.onDoublePressHandler([this]()
                                         {
                                             Serial.println("Timer State: Button Double Pressed / 计时状态：双击");

                                             // Send 'Stop' webhook (canceled)
                                             networkController.sendWebhookAction("stop");
                                             displayController.showCancel();
                                             stateMachine.changeState(&StateMachine::idleState); // Transition to IdleState / 返回空闲
                                         });

    networkController.startBluetooth();
    networkController.sendWebhookAction("start");
}

void TimerState::update()
{
    inputController.update();
    ledController.update();

    unsigned long currentTime = millis();
    elapsedTime = (currentTime - startTime) / 1000;

    int remainingSeconds = duration * 60 - elapsedTime;

    displayController.drawTimerScreen(remainingSeconds);

    // Check if the timer is done / 检查计时是否完成
    if (remainingSeconds <= 0)
    {
        Serial.println("Timer State: Done / 计时状态：完成");
        displayController.showTimerDone();
        stateMachine.changeState(&StateMachine::doneState); // Transition to Done State / 切换到完成状态
    }
}

void TimerState::exit()
{
    inputController.releaseHandlers();
    networkController.stopBluetooth();
    ledController.turnOff();
    Serial.println("Exiting Timer State / 离开计时状态");
}

void TimerState::setTimer(int duration, unsigned long elapsedTime)
{
    this->duration = duration;
    this->elapsedTime = elapsedTime;
}

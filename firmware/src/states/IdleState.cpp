#include "StateMachine.h"
#include "Controllers.h"

IdleState::IdleState() : defaultDuration(0), lastActivity(0)
{

    if (nvs_flash_init() != ESP_OK)
    {
        Serial.println("NVS Flash Init Failed / NVS 初始化失败");
    }
    else
    {
        Serial.println("NVS initialized successfully. / NVS 初始化成功");
    }

    // Load the default duration / 读取默认定时
    if (preferences.begin("focusdial", true))
    {
        defaultDuration = preferences.getInt("timer", DEFAULT_TIMER);
        preferences.end();
    }
}

void IdleState::enter()
{
    Serial.println("Entering Idle State / 进入空闲状态");
    ledController.setBreath(BLUE, -1, false, 5);

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([this]()
                                   {
                                       Serial.println("Idle State: Button pressed / 空闲状态：按键按下");
                                       StateMachine::timerState.setTimer(this->defaultDuration, 0);
                                       displayController.showTimerStart();
                                       stateMachine.changeState(&StateMachine::timerState); // Start timer / 切换到计时状态
                                   });

    inputController.onLongPressHandler([this]()
                                       {
                                           Serial.println("Idle State: Button long pressed / 空闲状态：长按");
                                           stateMachine.changeState(&StateMachine::resetState); // Transition to Reset State / 切换到重置状态
                                       });

    inputController.onEncoderRotateHandler([this](int delta)
                                           {
                                               Serial.println("Idle State: Encoder turned / 空闲状态：旋钮转动");
                                               StateMachine::adjustState.adjustTimer(this->defaultDuration);
                                               stateMachine.changeState(&StateMachine::adjustState); // Transition to Adjust State / 切换到调整状态
                                           });

    lastActivity = millis(); // Activity timer / 记录最近操作时间
}

void IdleState::update()
{
    static unsigned long lastUpdateTime = 0;

    // Controllers updates / 控制器轮询
    inputController.update();
    ledController.update();
    networkController.update();

    displayController.drawIdleScreen(defaultDuration, networkController.isWiFiConnected());

    // Check if sleep timeout is reached / 是否达到休眠超时
    if (millis() - lastActivity >= (SLEEP_TIMOUT * 60 * 1000))
    {
        Serial.println("Idle State: Activity timeout / 空闲状态：超时进入休眠");
        stateMachine.changeState(&StateMachine::sleepState); // Transition to Sleep State / 切换到休眠状态
    }
}

void IdleState::exit()
{
    Serial.println("Exiting Idle State / 离开空闲状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}

void IdleState::setTimer(int duration)
{
    defaultDuration = duration;

    preferences.begin("focusdial", true);
    preferences.putInt("timer", defaultDuration);
    preferences.end();
}

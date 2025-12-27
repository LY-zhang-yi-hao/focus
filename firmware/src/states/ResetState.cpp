#include "StateMachine.h"
#include "Controllers.h"

bool resetSelected = false; // button selection / 当前按钮选择状态

void ResetState::enter()
{
    Serial.println("Entering Reset State / 进入重置状态");

    ledController.setBreath(MAGENTA, -1, false, 10);

    // Register state-specific handlers / 注册状态回调
    inputController.onEncoderRotateHandler([this](int delta)
                                           {
        if (delta > 0) {
            resetSelected = true;  // Select "RESET" / 选择重置
        } else if (delta < 0) {
            resetSelected = false;  // Select "CANCEL" / 选择取消
        } });

    inputController.onPressHandler([this]()
                                   {
        if (resetSelected) {
            Serial.println("Reset State: RESET button pressed, rebooting. / 重置状态：确认重置，准备重启");
            displayController.showReset();
            networkController.reset();
            resetStartTime = millis();
        } else {
            Serial.println("Reset State: CANCEL button pressed, returning to Idle. / 重置状态：取消，返回空闲");
            displayController.showCancel();
            stateMachine.changeState(&StateMachine::idleState);
        } });
}

void ResetState::update()
{

    inputController.update();
    ledController.update();
    displayController.drawResetScreen(resetSelected);

    if (resetStartTime > 0 && (millis() - resetStartTime >= 1000))
    {
        Serial.println("Restarting ... / 正在重启");
        ESP.restart(); // Restart after 1 second / 1 秒后重启
    }
}

void ResetState::exit()
{
    Serial.println("Exiting Reset State / 离开重置状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}

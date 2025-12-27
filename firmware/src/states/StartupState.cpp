#include "StateMachine.h"
#include "Controllers.h"

StartupState::StartupState() : startEnter(0) {}

void StartupState::enter()
{
    Serial.println("Entering Splash State / 进入启动闪屏状态");

    displayController.drawSplashScreen();
    ledController.setSpinner(TEAL, -1);

    startEnter = millis();
}

void StartupState::update()
{
    ledController.update();

    if (millis() - startEnter >= (SPLASH_DURATION * 1000))
    {
        if (networkController.isWiFiProvisioned())
        {
            stateMachine.changeState(&StateMachine::idleState); // Transition to Idle / 进入空闲状态
        }
        else
        {
            stateMachine.changeState(&StateMachine::provisionState); // Trigger Provision / 进入配网状态
        }
    }
}

void StartupState::exit()
{
    ledController.turnOff();
    Serial.println("Exiting Splash State / 离开启动闪屏状态");
}

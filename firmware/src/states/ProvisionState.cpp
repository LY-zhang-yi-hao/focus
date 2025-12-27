#include "StateMachine.h"
#include "Controllers.h"

void ProvisionState::enter()
{
    Serial.println("Entering Provision State / 进入配网状态");
    inputController.releaseHandlers();
    displayController.drawProvisionScreen();
    ledController.setSolid(AMBER);
    networkController.startProvisioning();
}

void ProvisionState::update()
{
    ledController.update();
    if (networkController.isWiFiProvisioned() && networkController.isWiFiConnected())
    {
        Serial.println("Provisioning Complete, WiFi Connected / 配网完成，WiFi 已连接");
        displayController.showConnected();
        networkController.stopProvisioning();
        stateMachine.changeState(&StateMachine::idleState);
    }
}

void ProvisionState::exit()
{
    Serial.println("Exiting Provision State / 离开配网状态");
    networkController.stopProvisioning();
}

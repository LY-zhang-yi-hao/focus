#include <Arduino.h>
#include "Config.h"
#include "StateMachine.h"
#include "Controllers.h"

// Global instances of controllers / 控制器全局实例
DisplayController displayController(OLED_WIDTH, OLED_HEIGHT, OLED_ADDR);
LEDController ledController(LED_PIN, NUM_LEDS, LED_BRIGHTNESS);
InputController inputController(BUTTON_PIN, ENCODER_A_PIN, ENCODER_B_PIN);
NetworkController networkController;
Preferences preferences;

void setup() {
    Serial.begin(115200);
    
    // Initialize controllers / 初始化各控制器
    inputController.begin();
    displayController.begin();
    ledController.begin();
    networkController.begin();

    // Startup state / 进入启动状态
    stateMachine.changeState(&StateMachine::startupState);
}

void loop() {
    // Update state machine / 轮询状态机
    stateMachine.update();
    // If any animation needs to run / 若有动画需要更新则刷新
    displayController.updateAnimation();
}

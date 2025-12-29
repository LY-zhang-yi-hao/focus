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

    // Register HTTP API callbacks / 注册 HTTP API 回调
    networkController.setTaskListUpdateCallback([](const String& jsonTaskList) {
        Serial.println("Callback: Received task list / 回调：收到任务列表");
        StateMachine::taskListState.updateTaskList(jsonTaskList);

        // 静默更新任务列表：仅刷新缓存，不自动切换界面（避免推送后直接跳转到任务清单）
    });

    // Startup state / 进入启动状态
    stateMachine.changeState(&StateMachine::startupState);
}

void loop() {
    // Always handle HTTP API requests regardless of state / 无论何种状态都处理 HTTP API 请求
    networkController.update();
    // Update state machine / 轮询状态机
    stateMachine.update();
    // If any animation needs to run / 若有动画需要更新则刷新
    displayController.updateAnimation();
}

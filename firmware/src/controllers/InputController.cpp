#include "controllers/InputController.h"
#include <Arduino.h>

static InputController *instancePtr = nullptr; // Global pointer for the ISR / ISR 使用的全局指针

void InputController::handleEncoderInterrupt()
{
    if (instancePtr)
    {
        instancePtr->encoder.tick();
    }
}

void InputController::handleButtonInterrupt()
{
    if (instancePtr)
    {
        instancePtr->button.tick();
    }
}

InputController::InputController(uint8_t buttonPin, uint8_t encoderPinA, uint8_t encoderPinB)
    : button(buttonPin, true),
      encoder(encoderPinA, encoderPinB, RotaryEncoder::LatchMode::TWO03),
      lastPosition(0),
      buttonPin(buttonPin),
      encoderPinA(encoderPinA),
      encoderPinB(encoderPinB)
{

    // Attach click, double-click, and long-press handlers using OneButton library / 通过 OneButton 绑定单击、双击、长按回调
    button.attachClick([](void *scope)
                       { static_cast<InputController *>(scope)->onButtonClick(); }, this);
    button.attachDoubleClick([](void *scope)
                             { static_cast<InputController *>(scope)->onButtonDoubleClick(); }, this);
    button.attachLongPressStart([](void *scope)
                                { static_cast<InputController *>(scope)->onButtonLongPress(); }, this);

    instancePtr = this; // Set the global instance pointer to this instance / 记录当前实例供中断使用
}

void InputController::begin()
{
    button.setDebounceMs(20);
    button.setClickMs(150);
    button.setPressMs(400);
    lastPosition = encoder.getPosition();

    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(encoderPinA, INPUT_PULLUP);
    pinMode(encoderPinB, INPUT_PULLUP);

    // Set up interrupts for encoder handling / 配置编码器中断
    attachInterrupt(digitalPinToInterrupt(encoderPinA), handleEncoderInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPinB), handleEncoderInterrupt, CHANGE);

    // Set up interrupt for button handling / 配置按键中断
    attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonInterrupt, CHANGE); // Interrupt on button state change / 按键状态变化触发
}

void InputController::update()
{
    button.tick();
    encoder.tick();

    // Check encoder position and calculate delta / 计算编码器增量
    int currentPosition = encoder.getPosition();
    int delta = currentPosition - lastPosition;

    if (delta != 0)
    {
        onEncoderRotate(delta);
        lastPosition = currentPosition;
    }
}

// Register state-specific handlers / 注册当前状态的处理器
void InputController::onPressHandler(std::function<void()> handler)
{
    pressHandler = handler;
}

void InputController::onDoublePressHandler(std::function<void()> handler)
{
    doublePressHandler = handler;
}

void InputController::onLongPressHandler(std::function<void()> handler)
{
    longPressHandler = handler;
}

void InputController::onEncoderRotateHandler(std::function<void(int delta)> handler)
{
    encoderRotateHandler = handler;
}

// Method to release all handlers / 清空所有回调
void InputController::releaseHandlers()
{
    pressHandler = nullptr;
    doublePressHandler = nullptr;
    longPressHandler = nullptr;
    encoderRotateHandler = nullptr;

    button.reset();                       // Reset button state machine / 重置按键状态机
    lastPosition = encoder.getPosition(); // Reset encoder position tracking / 重置编码器位置记录
}

// Internal event handlers that call the registered state handlers / 内部事件转发到状态回调
void InputController::onButtonClick()
{
    if (pressHandler != nullptr)
    {
        pressHandler();
    }
}

void InputController::onButtonDoubleClick()
{
    if (doublePressHandler != nullptr)
    {
        doublePressHandler();
    }
}

void InputController::onButtonLongPress()
{
    if (longPressHandler != nullptr)
    {
        longPressHandler();
    }
}

void InputController::onEncoderRotate(int delta)
{
    if (encoderRotateHandler != nullptr)
    {
        encoderRotateHandler(delta); // Pass delta to the handler / 将增量传递给回调
    }
}

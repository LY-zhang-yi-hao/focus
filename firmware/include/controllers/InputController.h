#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <RotaryEncoder.h>
#include <functional>

class InputController
{
public:
    InputController(uint8_t buttonPin, uint8_t encoderPinA, uint8_t encoderPinB);
    void begin();
    void update();

    void onPressHandler(std::function<void()> handler);
    void onDoublePressHandler(std::function<void()> handler);
    void onLongPressHandler(std::function<void()> handler);
    void onEncoderRotateHandler(std::function<void(int delta)> handler);

    void releaseHandlers();

private:
    OneButton button;
    RotaryEncoder encoder;

    uint8_t buttonPin;
    uint8_t encoderPinA;
    uint8_t encoderPinB;

    std::function<void()> pressHandler = nullptr;
    std::function<void()> doublePressHandler = nullptr;
    std::function<void()> longPressHandler = nullptr;
    std::function<void(int delta)> encoderRotateHandler = nullptr;

    int lastPosition;

    void onButtonClick();
    void onButtonDoubleClick();
    void onButtonLongPress();
    void onEncoderRotate(int delta);

    static void handleEncoderInterrupt();
    static void handleButtonInterrupt();
};

extern InputController inputController;

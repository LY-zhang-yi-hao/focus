#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Animation.h"

class DisplayController
{
public:
    DisplayController(uint8_t oledWidth, uint8_t oledHeight, uint8_t oledAddress = 0x3C);

    void begin();

    void drawSplashScreen();
    void drawIdleScreen(int duration, bool wifi);
    void drawTimerScreen(int remainingSeconds);
    void drawPausedScreen(int remainingSeconds);
    void drawResetScreen(bool resetSelected);
    void drawDoneScreen();
    void drawAdjustScreen(int duration);
    void drawProvisionScreen();
    void clear();

    void showAnimation(const byte frames[][288], int frameCount, bool loop = false, bool reverse = false, unsigned long durationMs = 0, int width = 48, int height = 48);
    void updateAnimation();
    bool isAnimationRunning();

    void showConfirmation();
    void showCancel();
    void showReset();
    void showConnected();
    void showTimerDone();
    void showTimerStart();
    void showTimerPause();
    void showTimerResume();

private:
    Adafruit_SSD1306 oled;
    Animation animation;
};

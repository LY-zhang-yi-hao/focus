#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DEFAULT_FRAME_WIDTH 48
#define DEFAULT_FRAME_HEIGHT 48
#define DEFAULT_FRAME_DELAY 42

class Animation
{
public:
    Animation(Adafruit_SSD1306 *display);
    void start(const byte *frames, int frameCount, bool loop, bool reverse, unsigned long durationMs, int width, int height); // reverse 参数移至此处 / Moved reverse parameter
    void update();
    bool isRunning();

private:
    Adafruit_SSD1306 *oled;
    const byte *animationFrames;
    int totalFrames;
    int currentFrame;
    int frameWidth;
    int frameHeight;
    int frameX;
    int frameY;
    bool animationRunning;
    bool loopAnimation;
    bool playInReverse;
    unsigned long animationStartTime;
    unsigned long lastFrameTime;
    unsigned long animationDuration;
    unsigned long frameDelay;
};

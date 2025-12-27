#include "Animation.h"

Animation::Animation(Adafruit_SSD1306* display) : oled(display), animationRunning(false), playInReverse(false) {}

void Animation::start(const byte* frames, int frameCount, bool loop, bool reverse, unsigned long durationMs, int width, int height) {
    animationFrames = frames;
    totalFrames = frameCount;
    loopAnimation = loop;
    playInReverse = reverse; // Set reverse playback flag / 设置反向播放标志
    animationRunning = true;

    // Initialize current frame correctly based on direction / 根据方向初始化当前帧
    currentFrame = playInReverse ? totalFrames - 1 : 0;

    frameWidth = width;
    frameHeight = height;
    frameDelay = DEFAULT_FRAME_DELAY;

    if (durationMs == 0) {
        animationDuration = totalFrames * frameDelay;
    } else {
        animationDuration = durationMs;
    }

    animationStartTime = millis();
    lastFrameTime = millis();

    frameX = (oled->width() - frameWidth) / 2;
    // Move animation down to avoid yellow area (top 1/5 of screen ~13px)
    // 将动画向下移动以避开黄色区域(屏幕上方约1/5,约13像素)
    frameY = (oled->height() - frameHeight) / 2 + 10;

    oled->clearDisplay();
    oled->drawBitmap(frameX, frameY, &animationFrames[currentFrame * 288], frameWidth, frameHeight, 1);
    oled->display();
}

void Animation::update() {
    if (!animationRunning) return;

    unsigned long currentTime = millis();

    if (currentTime - animationStartTime >= animationDuration) {
        animationRunning = false;
        return;
    }

    // Check if it's time to advance to the next frame / 判断是否需要切换到下一帧
    if (currentTime - lastFrameTime >= frameDelay) {
        lastFrameTime = currentTime;

        // Adjust current frame based on direction / 按方向更新当前帧
        if (playInReverse) {
            currentFrame--;
            if (currentFrame < 0) { 
                if (loopAnimation) {
                    currentFrame = totalFrames - 1; // Wrap around to last frame / 回到最后一帧
                } else {
                    animationRunning = false;
                    return;
                }
            }
        } else {
            currentFrame++;
            if (currentFrame >= totalFrames) {
                if (loopAnimation) {
                    currentFrame = 0; // Wrap around to first frame / 回到第一帧
                } else {
                    animationRunning = false;
                    return;
                }
            }
        }

        // Display the current frame / 绘制当前帧
        oled->clearDisplay();
        oled->drawBitmap(frameX, frameY, &animationFrames[currentFrame * 288], frameWidth, frameHeight, 1);
        oled->display();
    }
}

bool Animation::isRunning() {
    return animationRunning;
}

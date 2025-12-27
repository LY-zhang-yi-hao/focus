#include "controllers/LedController.h"

LEDController::LEDController(uint8_t ledPin, uint16_t numLeds, uint8_t brightness)
    : leds(numLeds, ledPin),
      numLeds(numLeds),
      brightness(brightness),
      currentAnimation(None),
      lastUpdateTime(0),
      currentStep(0),
      currentCycle(0),
      decayStarted(false) {}

void LEDController::begin()
{
    leds.begin();
    leds.setBrightness(brightness);
    leds.show();
}

void LEDController::update()
{
    switch (currentAnimation)
    {
    case FillAndDecay:
        handleFillAndDecay();
        break;
    case Spinner:
        handleSpinner();
        break;
    case Breath:
        handleBreath();
        break;
    default:
        break;
    }
}

void LEDController::startFillAndDecay(uint32_t color, uint32_t totalDuration)
{
    stopCurrentAnimation();
    currentAnimation = FillAndDecay;
    animationColor = color;
    animationDuration = totalDuration;
    currentStep = 0;
    pixelIndex = 0;
    brightnessLevel = brightness;
    lastUpdateTime = millis();
}

void LEDController::setSpinner(uint32_t color, int cycles)
{
    stopCurrentAnimation();
    currentAnimation = Spinner;
    animationColor = color;
    animationCycles = cycles;
    currentCycle = 0;
    currentStep = 0;
    lastUpdateTime = millis();
}

void LEDController::setBreath(uint32_t color, int cycles, bool endFilled, uint32_t speed)
{
    stopCurrentAnimation();
    currentAnimation = Breath;
    animationColor = color;
    animationCycles = cycles;
    this->endFilled = endFilled;
    animationSpeed = speed;
    currentCycle = 0;
    currentStep = 0;
    lastUpdateTime = millis();
}

void LEDController::setSolid(uint32_t color)
{
    stopCurrentAnimation();
    leds.fill(color);
    leds.show();
}

void LEDController::turnOff()
{
    stopCurrentAnimation();
    leds.clear();
    leds.show();
}

uint32_t LEDController::scaleColor(uint32_t color, uint8_t brightnessLevel)
{
    uint8_t r = (color >> 16 & 0xFF) * brightnessLevel / 255;
    uint8_t g = (color >> 8 & 0xFF) * brightnessLevel / 255;
    uint8_t b = (color & 0xFF) * brightnessLevel / 255;
    return leds.Color(r, g, b);
}

void LEDController::handleFillAndDecay()
{
    uint32_t fillDuration = 300; // Initial fill duration / 初始快速填充时长
    uint32_t decayDuration = animationDuration - fillDuration;
    uint32_t totalSteps = (numLeds + 1) * brightness;
    uint32_t stepDuration = decayDuration / totalSteps;

    if (currentStep < numLeds)
    {
        // Quick fill phase / 快速填充阶段
        uint32_t stepDurationFill = fillDuration / numLeds;
        if (millis() - lastUpdateTime >= stepDurationFill)
        {
            leds.setPixelColor(currentStep, scaleColor(animationColor, brightness));
            leds.show();
            currentStep++;
            lastUpdateTime = millis();
        }
    }
    else
    {
        // Initialize decay phase / 初始化衰减阶段
        if (!decayStarted)
        {
            decayStarted = true;
            pixelIndex = 1;
            brightnessLevel = brightness;
            lastUpdateTime = millis();
        }

        // Decay phase / 衰减阶段
        if (millis() - lastUpdateTime >= stepDuration)
        {
            lastUpdateTime = millis();

            if (brightnessLevel > 0)
            {
                brightnessLevel--;
                leds.setPixelColor(pixelIndex, scaleColor(animationColor, brightnessLevel));
                leds.show();
            }
            else
            {
                leds.setPixelColor(pixelIndex, 0);
                leds.show();
                pixelIndex++;
                brightnessLevel = brightness;
            }

            if (pixelIndex > numLeds)
            {
                stopCurrentAnimation();
            }
        }
    }
}

void LEDController::handleSpinner()
{
    uint32_t stepDuration = 100;
    if (millis() - lastUpdateTime >= stepDuration)
    {
        leds.clear();
        for (int i = 0; i < numLeds; i++)
        {
            leds.setPixelColor((i + currentStep) % numLeds, scaleColor(animationColor, i * 255 / numLeds));
        }
        leds.show();
        currentStep++;
        lastUpdateTime = millis();

        if (currentStep >= numLeds)
        {
            currentStep = 0;
            currentCycle++;
            if (animationCycles != -1 && currentCycle >= animationCycles)
            {
                stopCurrentAnimation();
            }
        }
    }
}

void LEDController::handleBreath()
{
    if (millis() - lastUpdateTime >= animationSpeed)
    {
        // Calculate brightness with minimum threshold to prevent complete dimming
        // 计算亮度，设置最小阈值避免完全熄灭
        uint8_t rawBrightness = (currentStep <= 127) ? currentStep * 2 : (255 - currentStep) * 2;
        const uint8_t minBrightness = 25; // Minimum brightness ~10% (避免完全熄灭的最小亮度)
        uint8_t fadeBrightness = map(rawBrightness, 0, 255, minBrightness, 255);

        for (int i = 0; i < numLeds; i++)
        {
            leds.setPixelColor(i, scaleColor(animationColor, fadeBrightness));
        }
        leds.show();
        currentStep++;

        if (currentStep >= 255)
        {
            currentStep = 0;
            currentCycle++;

            // Adjust the number of cycles if `endFilled` is true / 若 endFilled 为真，最后补半个循环填充
            int effectiveCycles = animationCycles;
            if (endFilled && effectiveCycles > 0)
            {
                effectiveCycles--;
            }

            if (effectiveCycles != -1 && currentCycle >= effectiveCycles)
            {
                if (endFilled)
                {
                    // Additional half cycle to fill the LEDs / 额外半个循环补满 LED
                    for (int i = 0; i < numLeds; i++)
                    {
                        leds.setPixelColor(i, animationColor);
                    }
                    leds.show();
                }
                else
                {
                    turnOff();
                }
                stopCurrentAnimation();
            }
        }
        lastUpdateTime = millis();
    }
}

void LEDController::stopCurrentAnimation()
{
    currentAnimation = None;
    currentStep = 0;
    currentCycle = 0;
    pixelIndex = 0;
    brightnessLevel = brightness;
    decayStarted = false;
    lastUpdateTime = millis();
}

void LEDController::printDebugInfo()
{
    Serial.printf("Anim: %d, Step: %d, Cycle: %d, PixelIdx: %d, Leds numb: %d, Brightness: %d, Color: 0x%06X, Dur: %lu, Speed: %lu, Cycles: %d, EndFilled: %d / 动画调试信息\n",
                  currentAnimation, currentStep, currentCycle, pixelIndex, numLeds, brightness, animationColor, animationDuration, animationSpeed, animationCycles, endFilled);
}

#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "Animation.h"
#include "UIAnimation.h"
#include "models/FocusProject.h"
#include "models/FocusTask.h"
#include <vector>

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
    void drawTaskListScreen(const String& projectName, const std::vector<FocusTask>& tasks, int selectedIndex, int displayOffset, bool showingCompleted);
    void drawTaskListViewScreen(const String& projectName, const std::vector<FocusTask>& tasks, int selectedIndex, int displayOffset, bool showingCompleted);
    void drawProjectSelectScreen(const std::vector<FocusProject>& projects, int selectedIndex, int displayOffset, const String& selectedProjectId, bool readOnly);
    void drawTaskDetailScreen(const String& projectName, const FocusTask& task, int selectedIndex, int displayOffset);
    void drawDurationSelectScreen(const String& taskName, int duration);
    void drawTaskCompletePromptScreen(const String& taskName, bool markDoneSelected, bool isCanceled);
    void clear();

    // Animated task list drawing methods（丝滑翻页/滚动）
    void drawTaskListScreenAnimated(
        const String& projectName,
        const std::vector<FocusTask>& tasks,
        int selectedIndex,
        int displayOffset,
        bool showingCompleted,
        const TaskListAnimationState& animState
    );
    void drawTaskListViewScreenAnimated(
        const String& projectName,
        const std::vector<FocusTask>& tasks,
        int selectedIndex,
        int displayOffset,
        bool showingCompleted,
        const TaskListAnimationState& animState
    );

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
    U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
    Animation animation;

    // Internal helper methods for animated drawing
    void drawSegmentControlAnimated(
        int segX, int segY, int segW, int segH, int segR,
        bool showingCompleted,
        float highlightX
    );
    void drawScrollBarAnimated(
        int x, int y, int w, int h,
        float knobY,
        int knobH
    );
};

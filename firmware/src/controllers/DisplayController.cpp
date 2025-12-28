#include "controllers/DisplayController.h"

#include "fonts/Picopixel.h"
#include "fonts/Org_01.h"
#include "bitmaps.h"

// 判断字符串是否为可打印 ASCII（用于在小字库下避免中文渲染为空白）/ ASCII printable check
static bool isAsciiPrintable(const String& text) {
    for (size_t i = 0; i < text.length(); i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

DisplayController::DisplayController(uint8_t oledWidth, uint8_t oledHeight, uint8_t oledAddress)
    : oled(oledWidth, oledHeight, &Wire, -1), animation(&oled) {}

void DisplayController::begin() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);  // Loop forever if initialization fails
    }

    // oled.ssd1306_command(SSD1306_SETCONTRAST);
    // oled.ssd1306_command(128);
    
    oled.clearDisplay();
    oled.display();
    Serial.println("DisplayController initialized.");
}

void DisplayController::drawSplashScreen() {
    oled.clearDisplay();

    oled.drawBitmap(16, 3, focusdial_logo, 99, 45, 1);
    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(21, 60);
    oled.print("YOUTUBE/ @SALIMBENBOUZ");

    oled.display();
}

void DisplayController::drawIdleScreen(int duration, bool wifi) {
    if (isAnimationRunning()) return; 

    static unsigned long lastBlinkTime = 0;
    static bool blinkState = true; 

    unsigned long currentTime = millis();

    // Toggle blink state if WiFi is off
    if (!wifi && (currentTime - lastBlinkTime >= 500)) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
    }

    oled.clearDisplay();

    // "PRESS TO START"
    oled.setFont(&Picopixel);
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.setCursor(40, 58);
    oled.print("PRESS TO START");
    oled.drawRoundRect(35, 51, 60, 11, 1, 1);

    // Display WiFi icon based on WiFi state
    if (wifi) {
        oled.drawBitmap(70, 3, icon_wifi_on, 5, 5, 1); 
        oled.setCursor(54, 7);
        oled.print("WIFI");
    } else if (blinkState) {
        oled.drawBitmap(70, 3, icon_wifi_off, 5, 5, 1); 
        oled.setCursor(54, 7);
        oled.print("WIFI");
    }

    char left[3], right[3];
    int xLeft = 1; 
    int xRight = 73;

    if (duration < 60) {
        sprintf(left, "%02d", duration);
        strcpy(right, "00");
    } else {
        int hours = duration / 60;
        int minutes = duration % 60;
        sprintf(left, "%02d", hours);
        sprintf(right, "%02d", minutes);
    }

    // Adjust position if the first character is '1'
    if (left[0] == '1') {
        xLeft += 20;
    }
    if (right[0] == '1') {
        xRight += 20;
    }

    oled.setTextSize(5);
    oled.setFont(&Org_01);
    oled.setCursor(xLeft, 36);
    oled.print(left);

    oled.setCursor(xRight, 36);
    oled.print(right);

    // Separator dots
    oled.fillRect(62, 21, 5, 5, 1);
    oled.fillRect(62, 31, 5, 5, 1);

    oled.display();
}

void DisplayController::drawTimerScreen(int remainingSeconds) {
    if (isAnimationRunning()) return; 

    oled.clearDisplay();

    if (remainingSeconds < 0) {
        remainingSeconds = 0;
    }

    int hours = remainingSeconds / 3600;
    int minutes = (remainingSeconds % 3600) / 60;
    int seconds = remainingSeconds % 60;

    char left[3], right[3], secondsStr[3];
    int xLeft = 1; 
    int xRight = 73;

    // Format left and right
    if (hours > 0) {
        sprintf(left, "%02d", hours);
        sprintf(right, "%02d", minutes);
    } else {
        sprintf(left, "%02d", minutes);
        sprintf(right, "%02d", seconds);
    }

    // Adjust position if the first character is '1'
    if (left[0] == '1') {
        xLeft += 20;
    }
    if (right[0] == '1') {
        xRight += 20;
    }

    // Draw the left value (hours or minutes)
    oled.setTextColor(1);
    oled.setTextSize(5);
    oled.setFont(&Org_01);
    oled.setCursor(xLeft, 36);
    oled.print(left);

    // Draw the right value (minutes or seconds)
    oled.setCursor(xRight, 36);
    oled.print(right);

    // Separator dots
    oled.fillRect(62, 31, 5, 5, 1);
    oled.fillRect(62, 21, 5, 5, 1);

    sprintf(secondsStr, "%02d", seconds);

    int xSeconds = 54;
    if (secondsStr[0] == '1') {
        xSeconds += 8;  // Offset by 8 if the first char is '1'
    }

    oled.setTextSize(2);
    oled.setCursor(xSeconds, 58);
    oled.print(secondsStr);

    // Draw icons and labels
    oled.drawBitmap(61, 3, icon_star, 7, 7, 1);
    oled.setTextSize(1);
    oled.setCursor(27, 54);
    oled.print(hours > 0 ? "H" : "M");
    oled.setCursor(98, 54);
    oled.print(hours > 0 ? "M" : "S");

    oled.display();
}

void DisplayController::drawPausedScreen(int remainingSeconds) {
    if (isAnimationRunning()) return; 

    oled.clearDisplay();

    if (remainingSeconds < 0) {
        remainingSeconds = 0;
    }

    int hours = remainingSeconds / 3600;
    int minutes = (remainingSeconds % 3600) / 60;
    int seconds = remainingSeconds % 60;

    char left[3], right[3];
    int xLeft = 1;
    int xRight = 73;

    // Format left and right
    if (hours > 0) {
        sprintf(left, "%02d", hours);
        sprintf(right, "%02d", minutes);
    } else {
        sprintf(left, "%02d", minutes);
        sprintf(right, "%02d", seconds);
    }

    // Adjust position if the first character is '1'
    if (left[0] == '1') {
        xLeft += 20;
    }
    if (right[0] == '1') {
        xRight += 20;
    }

    if ((millis() / 400) % 2 == 0) {
        oled.setTextColor(1);
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setCursor(xLeft, 36);
        oled.print(left);
        oled.setCursor(xRight, 36);
        oled.print(right);

        oled.fillRect(62, 31, 5, 5, 1);
        oled.fillRect(62, 22, 5, 5, 1);

        oled.setFont(&Org_01);
        oled.setTextSize(1);
        oled.setCursor(27, 54);
        oled.print(hours > 0 ? "H" : "M");
        oled.setCursor(98, 54);
        oled.print(hours > 0 ? "M" : "S");
    }

    // Draw label and icon
    oled.drawRoundRect(47, 51, 35, 11, 1, 1);
    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(53, 58);
    oled.print("PAUSED");
    oled.drawBitmap(60, 2, icon_pause, 9, 9, 1);

    oled.display();
}

void DisplayController::drawResetScreen(bool resetSelected) {
    if (isAnimationRunning()) return; 
    oled.clearDisplay();

    // Static UI elements
    oled.setTextColor(1);
    oled.setTextSize(2);
    oled.setFont(&Picopixel);
    oled.setCursor(58, 15);
    oled.print("RESET");
    oled.setTextSize(1);
    oled.setCursor(20, 30);
    oled.print("ALL STORED SETTINGS WILL ");
    oled.setCursor(21, 40);
    oled.print("BE PERMANENTLY ERASED.");
    oled.drawBitmap(35, 4, icon_reset, 13, 16, 1);

    // Change only the rectangle fill and text color based on selection
    if (resetSelected) {
        // "RESET" filled, "CANCEL" outlined
        oled.fillRoundRect(67, 49, 37, 11, 1, 1); 
        oled.setTextColor(0);
        oled.setCursor(76, 56);
        oled.print("RESET");

        oled.drawRoundRect(24, 49, 37, 11, 1, 1);
        oled.setTextColor(1);
        oled.setCursor(31, 56);
        oled.print("CANCEL");
    } else {
        // "CANCEL" filled, "RESET" outlined
        oled.fillRoundRect(24, 49, 37, 11, 1, 1);
        oled.setTextColor(0);
        oled.setCursor(31, 56);
        oled.print("CANCEL");

        oled.drawRoundRect(67, 49, 37, 11, 1, 1); 
        oled.setTextColor(1);
        oled.setCursor(76, 56);
        oled.print("RESET");
    }

    oled.display();
}

void DisplayController::drawDoneScreen() {
    if (isAnimationRunning()) return;

    static unsigned long lastBlinkTime = 0;
    static bool blinkState = true;

    unsigned long currentTime = millis(); 

    // Toggle blink every 500 ms
    if (currentTime - lastBlinkTime >= 500) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime; 
    }

    oled.clearDisplay();

    if (blinkState) {
        oled.setTextColor(1);
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setCursor(1, 36);
        oled.print("00");
        oled.setCursor(73, 36);
        oled.print("00");
        oled.fillRect(62, 31, 5, 5, 1);
        oled.fillRect(62, 21, 5, 5, 1);
    }

    // Draw label and icon
    oled.fillRoundRect(46, 51, 35, 11, 1, 1);
    oled.setTextColor(0);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(56, 58);
    oled.print("DONE");
    oled.drawBitmap(61, 3, icon_star, 7, 7, 1);

    oled.display();
}


void DisplayController::drawAdjustScreen(int duration) {
    if (isAnimationRunning()) return; 

    oled.clearDisplay();

    oled.setTextColor(1);
    oled.setTextSize(4);
    oled.setFont(&Org_01);

    int hours = duration / 60;
    int minutes = duration % 60;

    char hourStr[3];
    char minuteStr[3];

    // Format hour and minute strings with leading zeros
    sprintf(hourStr, "%02d", hours);
    sprintf(minuteStr, "%02d", minutes);

    // Default positions for hours and minutes
    int xHour = 13;
    int xMinute = 72;

    // Check the first character and adjust position if '1'
    if (hourStr[0] == '1') {
        xHour += 15;
    }
    if (minuteStr[0] == '1') {
        xMinute += 15;
    }

    // Display hours
    oled.setCursor(xHour, 37);
    oled.print(hourStr);

    // Display minutes
    oled.setCursor(xMinute, 37);
    oled.print(minuteStr);

    // Display labels
    oled.setTextSize(1);
    oled.setCursor(26, 55);
    oled.print("HRS");
    oled.setCursor(86, 55);
    oled.print("MIN");

    // Additional UI elements
    oled.drawBitmap(0, 18, image_change_left, 7, 36, 1);   // 左侧括号位图
    oled.drawRoundRect(36, 1, 57, 11, 1, 1);
    oled.drawBitmap(121, 18, image_change_right, 7, 36, 1); // 右侧括号位图
    oled.setFont(&Picopixel);
    oled.setCursor(41, 8);
    oled.print("PRESS TO SAVE");
    oled.drawBitmap(103, 3, icon_arrow_down, 5, 7, 1);
    oled.drawBitmap(21, 3, icon_arrow_down, 5, 7, 1);

    oled.display();
}


void DisplayController::drawProvisionScreen() {
    if (isAnimationRunning()) return;

    oled.clearDisplay();

    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(12, 38);
    oled.print("PLEASE CONNECT TO BLUETOOTH");
    oled.setCursor(14, 48);
    oled.print("AND THIS FOCUSDIAL NETWORK");
    oled.setCursor(35, 58);
    oled.print("TO PROVISION WIFI");
    oled.drawBitmap(39, 4, provision_logo, 51, 23, 1);

    oled.display();
}

void DisplayController::drawTaskListScreen(const std::vector<FocusTask>& tasks, int selectedIndex, int displayOffset) {
    if (isAnimationRunning()) return;

    oled.clearDisplay();

    // Title / 标题
    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(34, 8);
    oled.print("SELECT TASK");

    // Empty state / 空列表提示
    if (tasks.empty()) {
        oled.setCursor(38, 30);
        oled.print("NO TASKS");
        oled.setCursor(22, 42);
        oled.print("PUSH FROM HA");
        oled.display();
        return;
    }

    const int START_Y = 16;
    const int LINE_HEIGHT = 14;
    const int MAX_VISIBLE = 3;

    // Clamp selection / 保护选中索引
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= (int)tasks.size()) selectedIndex = (int)tasks.size() - 1;

    for (int i = 0; i < MAX_VISIBLE && (displayOffset + i) < (int)tasks.size(); i++) {
        int taskIndex = displayOffset + i;
        int yPos = START_Y + (i * LINE_HEIGHT);

        if (taskIndex == selectedIndex) {
            oled.fillRect(0, yPos - 2, 128, LINE_HEIGHT, 1);
            oled.setTextColor(0);
        } else {
            oled.setTextColor(1);
        }

        // Name (truncate) / 任务名（截断）
        // 优先显示 displayName（HA 下发的 ASCII/拼音/简称），否则回退 name / Prefer displayName, fallback to name
        String name = tasks[taskIndex].displayName;
        if (name.isEmpty()) {
            name = tasks[taskIndex].name;
        }

        // 若包含非 ASCII 字符（如中文），在当前字体下可能显示为空白，提供兜底 / Fallback for non-ASCII
        if (name.isEmpty() || !isAsciiPrintable(name)) {
            String suffix = tasks[taskIndex].id;
            if (suffix.length() > 4) {
                suffix = suffix.substring(suffix.length() - 4);
            }
            if (!suffix.isEmpty() && isAsciiPrintable(suffix)) {
                name = "TASK " + suffix;
            } else {
                name = "TASK " + String(taskIndex + 1);
            }
        }

        if (name.length() > 16) {
            name = name.substring(0, 13) + "...";
        }
        oled.setCursor(4, yPos + 9);
        oled.print(name);
    }

    // Bottom info line for selected task / 底部显示选中任务信息（建议时长 + 今日累计）
    const FocusTask& selectedTask = tasks[selectedIndex];
    uint32_t todayMin = selectedTask.spentTodaySeconds / 60;
    uint32_t todayH = todayMin / 60;
    uint32_t todayM = todayMin % 60;

    char info[32];
    if (todayH > 0) {
        snprintf(info, sizeof(info), "D%dm T%luh%02lu",
                 selectedTask.estimatedDuration,
                 (unsigned long)todayH,
                 (unsigned long)todayM);
    } else {
        snprintf(info, sizeof(info), "D%dm T%lum",
                 selectedTask.estimatedDuration,
                 (unsigned long)todayMin);
    }

    oled.setTextColor(1);
    oled.setCursor(4, 62);
    oled.print(info);

    // Scroll indicators / 滚动提示
    if (displayOffset > 0) {
        oled.fillTriangle(124, 14, 120, 18, 128, 18, 1);
    }
    if (displayOffset + MAX_VISIBLE < (int)tasks.size()) {
        oled.fillTriangle(124, 62, 120, 58, 128, 58, 1);
    }

    oled.display();
}

void DisplayController::drawTaskCompletePromptScreen(const String& taskName, bool markDoneSelected, bool isCanceled) {
    if (isAnimationRunning()) return;

    oled.clearDisplay();

    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);

    // Title / 标题
    oled.setCursor(44, 8);
    oled.print(isCanceled ? "CANCELED" : "DONE");

    // Task name / 任务名
    String name = taskName;
    if (name.length() > 18) {
        name = name.substring(0, 15) + "...";
    }
    oled.setCursor(8, 24);
    oled.print(name);

    // Prompt / 提示文案
    oled.setCursor(36, 36);
    oled.print("MARK DONE?");

    // Options / 选项
    const int boxY = 46;
    const int boxW = 48;
    const int boxH = 14;
    const int yesX = 14;
    const int noX = 66;

    if (markDoneSelected) {
        oled.fillRoundRect(yesX, boxY, boxW, boxH, 2, 1);
        oled.drawRoundRect(noX, boxY, boxW, boxH, 2, 1);
        oled.setTextColor(0);
        oled.setCursor(32, boxY + 10);
        oled.print("YES");
        oled.setTextColor(1);
        oled.setCursor(86, boxY + 10);
        oled.print("NO");
    } else {
        oled.drawRoundRect(yesX, boxY, boxW, boxH, 2, 1);
        oled.fillRoundRect(noX, boxY, boxW, boxH, 2, 1);
        oled.setTextColor(1);
        oled.setCursor(32, boxY + 10);
        oled.print("YES");
        oled.setTextColor(0);
        oled.setCursor(86, boxY + 10);
        oled.print("NO");
        oled.setTextColor(1);
    }

    // Hint / 操作提示
    oled.setCursor(12, 62);
    oled.print("TURN=SEL PRESS=OK");

    oled.display();
}

void DisplayController::clear() {
    oled.clearDisplay();
    oled.display();
}

void DisplayController::showAnimation(const byte frames[][288], int frameCount, bool loop, bool reverse, unsigned long durationMs, int width, int height) {
    animation.start(&frames[0][0], frameCount, loop, reverse, durationMs, width, height); // Pass array as pointer
}

void DisplayController::updateAnimation() {
    animation.update();
}

bool DisplayController::isAnimationRunning() {
    return animation.isRunning();
}

void DisplayController::showConfirmation() {
    showAnimation(animation_tick, 20);
}

void DisplayController::showCancel() {
    showAnimation(animation_cancel, 18, false, true);
}

void DisplayController::showReset() {
    showAnimation(animation_reset, 28, true, false);
}

void DisplayController::showConnected() {
    showAnimation(animation_wifi, 28);
}

void DisplayController::showTimerStart() {
    showAnimation(animation_timer_start, 20, false, true);
}

void DisplayController::showTimerDone() {
    showAnimation(animation_timer_start, 20);
}

void DisplayController::showTimerPause() {
    showAnimation(animation_resume, 18, false, true);
}

void DisplayController::showTimerResume() {
    showAnimation(animation_resume, 18);
}

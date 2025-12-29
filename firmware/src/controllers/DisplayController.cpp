#include "controllers/DisplayController.h"

#include "fonts/Picopixel.h"
#include "fonts/Org_01.h"
#include "bitmaps.h"

// UTF-8 安全截断（避免中文被 substring 切断导致乱码）
static size_t utf8CharLen(unsigned char lead) {
    if ((lead & 0x80) == 0x00) return 1;        // 0xxxxxxx
    if ((lead & 0xE0) == 0xC0) return 2;        // 110xxxxx
    if ((lead & 0xF0) == 0xE0) return 3;        // 1110xxxx
    if ((lead & 0xF8) == 0xF0) return 4;        // 11110xxx
    return 1;
}

static String utf8Truncate(const String& text, size_t maxChars, bool addEllipsis = false) {
    if (maxChars == 0 || text.isEmpty()) {
        return "";
    }

    size_t i = 0;
    size_t count = 0;
    const size_t n = text.length();
    while (i < n && count < maxChars) {
        size_t step = utf8CharLen(static_cast<unsigned char>(text[i]));
        if (i + step > n) {
            break;
        }
        i += step;
        count++;
    }

    const bool truncated = i < n;
    String out = text.substring(0, i);
    if (truncated && addEllipsis) {
        out += "…";
    }
    return out;
}

DisplayController::DisplayController(uint8_t oledWidth, uint8_t oledHeight, uint8_t oledAddress)
    : oled(oledWidth, oledHeight, &Wire, -1), animation(&oled) {}

void DisplayController::begin() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);  // Loop forever if initialization fails
    }

    // 启用 U8g2 字库渲染（用于中文显示）
    u8g2Fonts.begin(oled);
    u8g2Fonts.setFontMode(1);          // 透明背景
    u8g2Fonts.setFontDirection(0);     // 正方向
    u8g2Fonts.setForegroundColor(1);   // 白色像素
    u8g2Fonts.setBackgroundColor(0);   // 黑色背景

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

    oled.setTextColor(1);
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
    oled.print("BE PERMANENTLY ERASED");
    oled.drawBitmap(35, 4, icon_reset, 13, 16, 1);

    // Change only the rectangle fill and text color based on selection
    const int boxY = 48;
    const int boxW = 37;
    const int boxH = 14;
    const int cancelX = 24;
    const int resetX = 67;

    if (resetSelected) {
        // "RESET" filled, "CANCEL" outlined
        oled.fillRoundRect(resetX, boxY, boxW, boxH, 1, 1);
        oled.setTextColor(0);
        oled.setCursor(76, 56);
        oled.print("RESET");

        oled.drawRoundRect(cancelX, boxY, boxW, boxH, 1, 1);
        oled.setTextColor(1);
        oled.setCursor(31, 56);
        oled.print("CANCEL");
    } else {
        // "CANCEL" filled, "RESET" outlined
        oled.fillRoundRect(cancelX, boxY, boxW, boxH, 1, 1);
        oled.setTextColor(0);
        oled.setCursor(31, 56);
        oled.print("CANCEL");

        oled.drawRoundRect(resetX, boxY, boxW, boxH, 1, 1);
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

    // 屏幕尺寸：128x64
    // 居中显示大号 "00:00"
    if (blinkState) {
        oled.setTextColor(1);
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        // 居中计算：字体宽度约 24px/字符，总宽度约 24*4+5=101，居中 x=(128-101)/2≈13
        oled.setCursor(13, 32);
        oled.print("00");
        oled.setCursor(85, 32);
        oled.print("00");
        // 分隔点居中
        oled.fillRect(64, 17, 5, 5, 1);
        oled.fillRect(64, 27, 5, 5, 1);
    }

    // "DONE" 标签居中显示
    // 框宽度 40px，居中 x=(128-40)/2=44
    const int boxW = 40;
    const int boxH = 14;
    const int boxX = (128 - boxW) / 2;  // 44
    const int boxY = 46;

    oled.fillRoundRect(boxX, boxY, boxW, boxH, 2, 1);

    // 使用中文字体显示"完成"
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(0);  // 黑色文字（反显）
    u8g2Fonts.setBackgroundColor(1);
    // 文字居中：框内居中
    u8g2Fonts.setCursor(boxX + 8, boxY + 11);
    u8g2Fonts.print("完成");

    // 顶部星星图标居中
    oled.drawBitmap(60, 3, icon_star, 7, 7, 1);

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
    // Display labels
    oled.setTextSize(1);
    oled.setCursor(26, 55);
    oled.print("HRS");
    oled.setCursor(86, 55);
    oled.print("MIN");

    // Additional UI elements
    oled.drawBitmap(0, 18, image_change_left, 7, 36, 1);
    oled.drawRoundRect(36, 1, 57, 11, 1, 1);
    oled.drawBitmap(121, 18, image_change_right, 7, 36, 1);
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

    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(1);
    u8g2Fonts.setBackgroundColor(0);
    u8g2Fonts.setCursor(18, 38);
    u8g2Fonts.print("请先连接蓝牙");
    u8g2Fonts.setCursor(14, 50);
    u8g2Fonts.print("再连接设备热点");
    u8g2Fonts.setCursor(18, 62);
    u8g2Fonts.print("完成无线配网");
    oled.drawBitmap(39, 4, provision_logo, 51, 23, 1);

    oled.display();
}

void DisplayController::drawTaskListScreen(const std::vector<FocusTask>& tasks, int selectedIndex, int displayOffset, bool showingCompleted) {
    if (isAnimationRunning()) return;

    oled.clearDisplay();

    // 标题
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(1);
    u8g2Fonts.setBackgroundColor(0);
    u8g2Fonts.setCursor(showingCompleted ? 46 : 40, 12);
    u8g2Fonts.print(showingCompleted ? "已完成" : "待办任务");

    // 空列表提示
    if (tasks.empty()) {
        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setForegroundColor(1);
        u8g2Fonts.setCursor(34, 34);
        u8g2Fonts.print(showingCompleted ? "暂无已完成" : "暂无待办");

        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setCursor(16, 52);
        u8g2Fonts.print(showingCompleted ? "完成后将自动同步" : "请从家庭助手推送");
        oled.display();
        return;
    }

    const int START_Y = 30;      // 任务行基线起点（配合 wqy12）
    const int LINE_HEIGHT = 16;  // 行高
    const int MAX_VISIBLE = 2;

    // 保护选中索引
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= (int)tasks.size()) selectedIndex = (int)tasks.size() - 1;

    for (int i = 0; i < MAX_VISIBLE && (displayOffset + i) < (int)tasks.size(); i++) {
        int taskIndex = displayOffset + i;
        int yPos = START_Y + (i * LINE_HEIGHT);

        if (taskIndex == selectedIndex) {
            oled.fillRect(0, yPos - 13, 128, LINE_HEIGHT, 1);
            u8g2Fonts.setForegroundColor(0);
            u8g2Fonts.setBackgroundColor(1);
        } else {
            u8g2Fonts.setForegroundColor(1);
            u8g2Fonts.setBackgroundColor(0);
        }

        // 任务名（UTF-8 安全截断）
        // 以 name 为主（支持中文），display_name 仅做兼容兜底
        String name = tasks[taskIndex].name;
        if (name.isEmpty()) {
            name = tasks[taskIndex].displayName;
        }

        if (name.isEmpty()) {
            String suffix = tasks[taskIndex].id;
            if (suffix.length() > 4) {
                suffix = suffix.substring(suffix.length() - 4);
            }
            name = suffix.isEmpty() ? "未命名任务" : ("任务 " + suffix);
        }

        name = utf8Truncate(name, 8, true);

        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setCursor(4, yPos);
        u8g2Fonts.print(name);
    }

    // 底部信息
    const FocusTask& selectedTask = tasks[selectedIndex];

    char info[64];
    if (showingCompleted) {
        // 已完成：显示完成日期 + 完成当天累计专注（分钟四舍五入）
        uint32_t displaySeconds = selectedTask.completedSpentSeconds;
        if (displaySeconds == 0) {
            // 兼容旧 payload：没有 completed_spent_sec 时，兜底使用 spent_today_sec
            displaySeconds = selectedTask.spentTodaySeconds;
        }
        uint32_t roundedMin = (displaySeconds + 30) / 60;

        if (!selectedTask.completedAt.isEmpty() && roundedMin > 0) {
            snprintf(info, sizeof(info), "%s,专注%lumin", selectedTask.completedAt.c_str(), (unsigned long)roundedMin);
        } else if (!selectedTask.completedAt.isEmpty()) {
            snprintf(info, sizeof(info), "%s,已完成", selectedTask.completedAt.c_str());
        } else if (roundedMin > 0) {
            snprintf(info, sizeof(info), "专注%lumin", (unsigned long)roundedMin);
        } else {
            snprintf(info, sizeof(info), "已完成");
        }
    } else {
        // 待办：显示今日累计
        uint32_t todayMin = selectedTask.spentTodaySeconds / 60;
        snprintf(info, sizeof(info), "今日已专注 %lumin", (unsigned long)todayMin);
    }

    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(1);
    u8g2Fonts.setBackgroundColor(0);
    u8g2Fonts.setCursor(16, 63);
    u8g2Fonts.print(info);

    // 滚动提示
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

    // 标题 - 使用 Picopixel 字体居中显示
    // "COMPLETED" 约 9 字符 × 4px = 36px，居中 x = (128-36)/2 = 46
    // "CANCELED" 约 8 字符 × 4px = 32px，居中 x = (128-32)/2 = 48
    oled.setFont(&Picopixel);
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.setCursor(isCanceled ? 48 : 46, 8);
    oled.print(isCanceled ? "CANCELED" : "COMPLETED");

    // 任务名 - 动态计算宽度居中显示
    String name = utf8Truncate(taskName, 8, true);
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(1);
    u8g2Fonts.setBackgroundColor(0);
    // 使用 getUTF8Width 获取实际文字宽度，实现真正居中
    int16_t nameWidth = u8g2Fonts.getUTF8Width(name.c_str());
    int16_t nameX = (128 - nameWidth) / 2;
    if (nameX < 0) nameX = 0;
    u8g2Fonts.setCursor(nameX, 26);
    u8g2Fonts.print(name);

    // 提示文案 - 居中
    // "Mark as done?" 约 13 字符 × 6px = 78px，居中 x = (128-78)/2 = 25
    u8g2Fonts.setCursor(25, 40);
    u8g2Fonts.print("Mark as done?");

    // 选项按钮
    const int boxY = 48;
    const int boxW = 48;
    const int boxH = 14;
    // 两个按钮总宽度 = 48 + 4(间距) + 48 = 100，居中起点 x = (128-100)/2 = 14
    const int yesX = 14;
    const int noX = 66;
    // YES 文字居中：框宽48，"YES"约18px，x = yesX + (48-18)/2 = 14 + 15 = 29
    // NO 文字居中：框宽48，"NO"约12px，x = noX + (48-12)/2 = 66 + 18 = 84
    const int yesTextX = yesX + 15;
    const int noTextX = noX + 18;
    const int textY = boxY + 11;

    if (markDoneSelected) {
        oled.fillRoundRect(yesX, boxY, boxW, boxH, 2, 1);
        oled.drawRoundRect(noX, boxY, boxW, boxH, 2, 1);
        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setForegroundColor(0);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setCursor(yesTextX, textY);
        u8g2Fonts.print("YES");
        u8g2Fonts.setForegroundColor(1);
        u8g2Fonts.setBackgroundColor(0);
        u8g2Fonts.setCursor(noTextX, textY);
        u8g2Fonts.print("NO");
    } else {
        oled.drawRoundRect(yesX, boxY, boxW, boxH, 2, 1);
        oled.fillRoundRect(noX, boxY, boxW, boxH, 2, 1);
        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setForegroundColor(1);
        u8g2Fonts.setBackgroundColor(0);
        u8g2Fonts.setCursor(yesTextX, textY);
        u8g2Fonts.print("YES");
        u8g2Fonts.setForegroundColor(0);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setCursor(noTextX, textY);
        u8g2Fonts.print("NO");
        u8g2Fonts.setForegroundColor(1);
        u8g2Fonts.setBackgroundColor(0);
    }

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

void DisplayController::drawDurationSelectScreen(const String& taskName, int duration) {
    if (isAnimationRunning()) return;

    oled.clearDisplay();

    // 与 drawAdjustScreen 保持一致的风格
    oled.setTextColor(1);
    oled.setTextSize(4);
    oled.setFont(&Org_01);

    int hours = duration / 60;
    int minutes = duration % 60;

    char hourStr[3];
    char minuteStr[3];

    sprintf(hourStr, "%02d", hours);
    sprintf(minuteStr, "%02d", minutes);

    // 默认位置
    int xHour = 13;
    int xMinute = 72;

    // 数字 '1' 较窄，需要调整位置
    if (hourStr[0] == '1') {
        xHour += 15;
    }
    if (minuteStr[0] == '1') {
        xMinute += 15;
    }

    // 显示小时
    oled.setCursor(xHour, 37);
    oled.print(hourStr);

    // 显示分钟
    oled.setCursor(xMinute, 37);
    oled.print(minuteStr);

    // 显示标签 HRS / MIN
    oled.setTextSize(1);
    oled.setCursor(26, 55);
    oled.print("HRS");
    oled.setCursor(86, 55);
    oled.print("MIN");

    // 左右箭头图标
    oled.drawBitmap(0, 18, image_change_left, 7, 36, 1);
    oled.drawBitmap(121, 18, image_change_right, 7, 36, 1);

    // 顶部提示框："PRESS TO START"
    oled.drawRoundRect(32, 1, 65, 11, 1, 1);
    oled.setFont(&Picopixel);
    oled.setCursor(36, 8);
    oled.print("PRESS TO START");

    // 顶部箭头图标
    oled.drawBitmap(103, 3, icon_arrow_down, 5, 7, 1);
    oled.drawBitmap(21, 3, icon_arrow_down, 5, 7, 1);

    oled.display();
}

void DisplayController::drawTaskListViewScreen(const std::vector<FocusTask>& tasks, int selectedIndex, int displayOffset, bool showingCompleted) {
    if (isAnimationRunning()) return;

    oled.clearDisplay();

    // 标题（带"查看中"标记）
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(1);
    u8g2Fonts.setBackgroundColor(0);

    // 显示"查看中"标记
    u8g2Fonts.setCursor(4, 12);
    u8g2Fonts.print("查看");
    u8g2Fonts.setCursor(showingCompleted ? 46 : 40, 12);
    u8g2Fonts.print(showingCompleted ? "已完成" : "待办任务");

    // 空列表提示
    if (tasks.empty()) {
        u8g2Fonts.setCursor(34, 34);
        u8g2Fonts.print(showingCompleted ? "暂无已完成" : "暂无待办");
        u8g2Fonts.setCursor(24, 52);
        u8g2Fonts.print("单击返回专注");
        oled.display();
        return;
    }

    const int START_Y = 30;
    const int LINE_HEIGHT = 16;
    const int MAX_VISIBLE = 2;

    // 保护选中索引
    int safeIndex = selectedIndex;
    if (safeIndex < 0) safeIndex = 0;
    if (safeIndex >= (int)tasks.size()) safeIndex = (int)tasks.size() - 1;

    for (int i = 0; i < MAX_VISIBLE && (displayOffset + i) < (int)tasks.size(); i++) {
        int taskIndex = displayOffset + i;
        int yPos = START_Y + (i * LINE_HEIGHT);

        if (taskIndex == safeIndex) {
            oled.fillRect(0, yPos - 13, 128, LINE_HEIGHT, 1);
            u8g2Fonts.setForegroundColor(0);
            u8g2Fonts.setBackgroundColor(1);
        } else {
            u8g2Fonts.setForegroundColor(1);
            u8g2Fonts.setBackgroundColor(0);
        }

        // 任务名
        String name = tasks[taskIndex].name;
        if (name.isEmpty()) {
            name = tasks[taskIndex].displayName;
        }
        if (name.isEmpty()) {
            String suffix = tasks[taskIndex].id;
            if (suffix.length() > 4) {
                suffix = suffix.substring(suffix.length() - 4);
            }
            name = suffix.isEmpty() ? "未命名任务" : ("任务 " + suffix);
        }
        name = utf8Truncate(name, 8, true);

        u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2Fonts.setFontMode(1);
        u8g2Fonts.setCursor(4, yPos);
        u8g2Fonts.print(name);
    }

    // 底部提示
    u8g2Fonts.setForegroundColor(1);
    u8g2Fonts.setBackgroundColor(0);
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setCursor(24, 63);
    u8g2Fonts.print("单击返回专注");

    // 滚动提示
    if (displayOffset > 0) {
        oled.fillTriangle(124, 14, 120, 18, 128, 18, 1);
    }
    if (displayOffset + MAX_VISIBLE < (int)tasks.size()) {
        oled.fillTriangle(124, 62, 120, 58, 128, 58, 1);
    }

    oled.display();
}

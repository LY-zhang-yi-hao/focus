#include "controllers/DisplayController.h"

#include "fonts/Picopixel.h"
#include "fonts/Org_01.h"
#include "bitmaps.h"

DisplayController::DisplayController(uint8_t oledWidth, uint8_t oledHeight, uint8_t oledAddress)
    : oled(oledWidth, oledHeight, &Wire, -1), animation(&oled) {}

void DisplayController::begin() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed / SSD1306 内存分配失败"));
        for (;;);  // Loop forever if initialization fails / 初始化失败则无限循环
    }

    // oled.ssd1306_command(SSD1306_SETCONTRAST);
    // oled.ssd1306_command(128);
    
    oled.clearDisplay();
    oled.display();
    Serial.println("DisplayController initialized. / 显示控制器初始化完成");
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

    // Toggle blink state if WiFi is off / WiFi 未连接时闪烁提示
    if (!wifi && (currentTime - lastBlinkTime >= 500)) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
    }

    oled.clearDisplay();

    // "PRESS TO START"（按下开始）
    oled.setFont(&Picopixel);
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.setCursor(40, 58);
    oled.print("PRESS TO START"); // 按下开始
    oled.drawRoundRect(35, 51, 60, 11, 1, 1);

    // Display WiFi icon based on WiFi state / 根据 WiFi 状态显示图标
    if (wifi) {
        oled.drawBitmap(70, 3, icon_wifi_on, 5, 5, 1); 
        oled.setCursor(54, 7);
        oled.print("WIFI"); // WiFi 已连接
    } else if (blinkState) {
        oled.drawBitmap(70, 3, icon_wifi_off, 5, 5, 1); 
        oled.setCursor(54, 7);
        oled.print("WIFI"); // WiFi 未连接
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

    // Adjust position if the first character is '1' / 首字符为 1 时调整位置
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

    // Separator dots / 分隔符圆点
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

    // Format left and right / 格式化左侧与右侧显示
    if (hours > 0) {
        sprintf(left, "%02d", hours);
        sprintf(right, "%02d", minutes);
    } else {
        sprintf(left, "%02d", minutes);
        sprintf(right, "%02d", seconds);
    }

    // Adjust position if the first character is '1' / 首字符为 1 时右移一点
    if (left[0] == '1') {
        xLeft += 20;
    }
    if (right[0] == '1') {
        xRight += 20;
    }

    // Draw the left value (hours or minutes) / 绘制左侧（小时或分钟）
    oled.setTextColor(1);
    oled.setTextSize(5);
    oled.setFont(&Org_01);
    oled.setCursor(xLeft, 36);
    oled.print(left);

    // Draw the right value (minutes or seconds) / 绘制右侧（分钟或秒）
    oled.setCursor(xRight, 36);
    oled.print(right);

    // Separator dots / 分隔符圆点
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

    // Draw icons and labels / 绘制图标与标签
    oled.drawBitmap(61, 3, icon_star, 7, 7, 1);
    oled.setTextSize(1);
    oled.setCursor(27, 54);
    oled.print(hours > 0 ? "H" : "M"); // H=小时 M=分钟
    oled.setCursor(98, 54);
    oled.print(hours > 0 ? "M" : "S"); // M=分钟 S=秒

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

    // Format left and right / 格式化左右显示值
    if (hours > 0) {
        sprintf(left, "%02d", hours);
        sprintf(right, "%02d", minutes);
    } else {
        sprintf(left, "%02d", minutes);
        sprintf(right, "%02d", seconds);
    }

    // Adjust position if the first character is '1' / 首字符为 1 时右移一点
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

    // Draw label and icon / 绘制标签与暂停图标
    oled.drawRoundRect(47, 51, 35, 11, 1, 1);
    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(53, 58);
    oled.print("PAUSED"); // 已暂停
    oled.drawBitmap(60, 2, icon_pause, 9, 9, 1);

    oled.display();
}

void DisplayController::drawResetScreen(bool resetSelected) {
    if (isAnimationRunning()) return; 
    oled.clearDisplay();

    // Static UI elements / 静态 UI 元素
    oled.setTextColor(1);
    oled.setTextSize(2);
    oled.setFont(&Picopixel);
    oled.setCursor(54, 15);
    oled.print("RESET"); // 重置
    oled.setTextSize(1);
    oled.setCursor(20, 30);
    oled.print("ALL STORED SETTINGS WILL "); // 所有存储的设置将会
    oled.setCursor(21, 40);
    oled.print("BE PERMANENTLY ERASED.");   // 被永久清除
    oled.drawBitmap(35, 4, icon_reset, 13, 16, 1);

    // Change only the rectangle fill and text color based on selection / 根据选择高亮对应按钮
    if (resetSelected) {
        // "RESET" filled, "CANCEL" outlined / 选中重置时填充 RESET、勾勒 CANCEL
        oled.fillRoundRect(67, 49, 37, 11, 1, 1); 
        oled.setTextColor(0);
        oled.setCursor(76, 56);
        oled.print("RESET"); // 确认重置

        oled.drawRoundRect(24, 49, 37, 11, 1, 1);
        oled.setTextColor(1);
        oled.setCursor(31, 56);
        oled.print("CANCEL"); // 取消
    } else {
        // "CANCEL" filled, "RESET" outlined / 选中取消时填充 CANCEL、勾勒 RESET
        oled.fillRoundRect(24, 49, 37, 11, 1, 1);
        oled.setTextColor(0);
        oled.setCursor(31, 56);
        oled.print("CANCEL"); // 取消

        oled.drawRoundRect(67, 49, 37, 11, 1, 1); 
        oled.setTextColor(1);
        oled.setCursor(76, 56);
        oled.print("RESET"); // 重置
    }

    oled.display();
}

void DisplayController::drawDoneScreen() {
    if (isAnimationRunning()) return;

    static unsigned long lastBlinkTime = 0;
    static bool blinkState = true;

    unsigned long currentTime = millis(); 

    // Toggle blink every 500 ms / 每 500 毫秒闪烁一次
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

    // Draw label and icon / 绘制完成标签与星标
    oled.fillRoundRect(46, 51, 35, 11, 1, 1);
    oled.setTextColor(0);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(56, 58);
    oled.print("DONE"); // 完成
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

    // Format hour and minute strings with leading zeros / 格式化小时与分钟，补零
    sprintf(hourStr, "%02d", hours);
    sprintf(minuteStr, "%02d", minutes);

    // Default positions for hours and minutes / 小时与分钟的默认位置
    int xHour = 13;
    int xMinute = 72;

    // Check the first character and adjust position if '1' / 如果首字符为 1 则右移
    if (hourStr[0] == '1') {
        xHour += 15;
    }
    if (minuteStr[0] == '1') {
        xMinute += 15;
    }

    // Display hours / 显示小时
    oled.setCursor(xHour, 37);
    oled.print(hourStr);

    // Display minutes / 显示分钟
    oled.setCursor(xMinute, 37);
    oled.print(minuteStr);

    // Display labels / 显示标签
    oled.setTextSize(1);
    oled.setCursor(26, 55);
    oled.print("HRS"); // 小时
    oled.setCursor(86, 55);
    oled.print("MIN"); // 分钟

    // Additional UI elements / 其他 UI 元素
    oled.drawBitmap(0, 21, image_change_left, 7, 40, 1);
    oled.drawRoundRect(36, 16, 57, 11, 1, 1);  // 放在黄色区域下方，时间数字上方
    oled.drawBitmap(121, 21, image_change_right, 7, 40, 1);
    oled.setFont(&Picopixel);
    oled.setCursor(41, 23);  // 对齐到圆角矩形内部
    oled.print("PRESS TO SAVE"); // 按下保存
    oled.drawBitmap(103, 18, icon_arrow_down, 5, 7, 1);  // 对齐到圆角矩形附近
    oled.drawBitmap(21, 18, icon_arrow_down, 5, 7, 1);   // 对齐到圆角矩形附近

    oled.display();
}


void DisplayController::drawProvisionScreen() {
    if (isAnimationRunning()) return; 

    oled.clearDisplay();

    oled.setTextColor(1);
    oled.setTextSize(1);
    oled.setFont(&Picopixel);
    oled.setCursor(12, 38);
    oled.print("PLEASE CONNECT TO BLUETOOTH"); // 请连接蓝牙
    oled.setCursor(14, 48);
    oled.print("AND THIS FOCUSDIAL NETWORK");  // 并加入 FocusDial WiFi
    oled.setCursor(35, 58);
    oled.print("TO PROVISION WIFI");           // 以完成 WiFi 配置
    oled.drawBitmap(39, 4, provision_logo, 51, 23, 1);

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

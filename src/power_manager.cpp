#include "power_manager.h"
#include "config.h"
#include <Arduino.h> // 用于 pinMode, digitalWrite, analogWrite, millis, Serial 等
// ui_manager.h 和 esp_now_handler.h 通过 power_manager.h 包含以获取 extern 声明

// --- 在此定义的全局状态变量 ---
bool isScreenOn = true;
bool hasNewUpdateWhileScreenOff = false;

// --- 用于电源管理的静态 (文件局部) 变量 ---
// LED 呼吸效果
static int breathBrightness = 0;
static int breathDirection = 5;
static unsigned long lastBreathTime = 0;

// BOOT 按钮处理
static unsigned long pressStartTime = 0;

// --- 函数实现 ---

void powerManagerInit() {
    pinMode(BUTTON_IO0, INPUT_PULLUP);
    pinMode(BATTERY_PIN, INPUT);
    pinMode(TFT_BL, OUTPUT); // TFT 背光控制

    // 初始化 LED
    pinMode(GREEN_LED, OUTPUT);
    analogWriteResolution(GREEN_LED, 8);
    analogWriteFrequency(GREEN_LED, 5000);
    analogWrite(GREEN_LED, 255); // 熄灭

    pinMode(BLUE_LED, OUTPUT);
    analogWriteResolution(BLUE_LED, 8);
    analogWriteFrequency(BLUE_LED, 5000);
    analogWrite(BLUE_LED, 255); // 熄灭

    pinMode(RED_LED, OUTPUT);
    analogWriteResolution(RED_LED, 8);
    analogWriteFrequency(RED_LED, 5000);
    analogWrite(RED_LED, 255); // 熄灭

    // 初始时点亮屏幕
    digitalWrite(TFT_BL, HIGH);
    isScreenOn = true;
}

void updateBreathLED() {
    // 此函数通常在 !isScreenOn && hasNewUpdateWhileScreenOff 时调用
    unsigned long currentTime = millis();
    if (currentTime - lastBreathTime >= 10) { // 呼吸效果的更新间隔
        lastBreathTime = currentTime;
        breathBrightness += breathDirection;
        if (breathBrightness >= 255 || breathBrightness <= 0) {
            breathDirection = -breathDirection;
            breathBrightness = constrain(breathBrightness, 0, 255);
        }
        // 假设绿色LED用于呼吸效果，亮度为255-val (共阴极或逻辑)
        analogWrite(GREEN_LED, 255 - breathBrightness);
    }
}

float readBatteryVoltagePercentage() {
    int adcValue = analogRead(BATTERY_PIN);
    float voltage = (adcValue / 4095.0) * 3.3; // ADC 参考电压 3.3V, 12位 ADC
    voltage *= 2;                              // 假设使用1:1分压器 (R1=R2)
    // 将电压映射到百分比 (根据您的特定电池调整这些值)
    // 典型锂聚合物电池: 3.0V (空) 到 4.2V (满)
    // 原始代码使用: 2.25V 到 3.9V
    float percentage = (voltage - 2.25) / (3.7 - 2.25) * 100.0;
    return constrain(percentage, 0, 100);
}

void toggleScreen() {
    if (isScreenOn) {
        Serial.println("关闭屏幕");
        digitalWrite(TFT_BL, LOW);
        isScreenOn = false;
    } else {
        Serial.println("打开屏幕");
        digitalWrite(TFT_BL, HIGH);
        isScreenOn = true;
        analogWrite(GREEN_LED, 255); // 如果呼吸灯亮着则将其关闭
        hasNewUpdateWhileScreenOff = false;
        if (!inCustomColorMode) { // inCustomColorMode 是来自 ui_manager 的 extern 变量
            drawDebugInfo();      // drawDebugInfo 是来自 ui_manager 的 extern 变量
        }
    }
}

void handleBootButton() {
    if (digitalRead(BUTTON_IO0) == LOW) { // 按钮按下
        if (pressStartTime == 0) { // 首次检测到按下
            pressStartTime = millis();
        }

        if (millis() - pressStartTime >= 2000) { // 长按 (2秒)
            Serial.println("检测到长按IO0按钮，进入深度睡眠模式...");
            // 如有必要，执行任何睡眠前清理工作
            // 例如：tft.fillScreen(TFT_BLACK); digitalWrite(TFT_BL, LOW);
            esp_deep_sleep_start();
            // 代码执行在此处停止以进入深度睡眠
        }
    } else { // 按钮未按下 (已释放或从未按下)
        if (pressStartTime > 0 && millis() - pressStartTime < 2000) { // 曾被按下，且为短按
            Serial.println("检测到短按IO0按钮");
            toggleScreen();
        }
        pressStartTime = 0; // 重置按下开始时间
    }
}

void manageScreenStateLEDs() {
    if (!isScreenOn) {
        // 屏幕关闭，管理指示灯
        // 如果 hasNewUpdateWhileScreenOff 为 true，则绿色LED的呼吸效果由 updateBreathLED 处理
        if (!hasNewUpdateWhileScreenOff) { // 如果没有呼吸效果，确保绿色LED熄灭
             analogWrite(GREEN_LED, 255); // 熄灭
        }

        // 蓝色LED用于ESP-NOW连接指示
        if (macSet.size() > 0) { // macSet 是来自 esp_now_handler 的 extern 变量
            analogWrite(BLUE_LED, 255 - BLUE_LED_DIM_DUTY_CYCLE); // 调暗蓝色LED
        } else {
            analogWrite(BLUE_LED, 255); // 蓝色LED熄灭
        }

        // 红色LED用于电源指示
        analogWrite(RED_LED, 255 - RED_LED_DIM_DUTY_CYCLE); // 调暗红色LED
    } else {
        // 屏幕开启，关闭这些指示灯 (或设置为默认亮起状态)
        analogWrite(GREEN_LED, 255); // 熄灭 (呼吸效果仅在屏幕关闭时)
        analogWrite(BLUE_LED, 255);  // 熄灭
        analogWrite(RED_LED, 255);   // 熄灭
    }
}

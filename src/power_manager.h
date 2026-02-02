#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "config.h"
#include <Arduino.h> // For types like unsigned long, bool

// --- 电源管理器所需的其他模块的 Extern 全局变量 ---
// extern TFT_eSPI tft; // 如果电源管理器直接控制 TFT_BL 则需要，但 digitalWrite 是通用的
// 用于 LED 指示中的 macSet.size():
#include "esp_now_handler.h" // 提供 extern std::set<String> macSet;
// 用于 handleBootButton 中的 drawDebugInfo() 和 inCustomColorMode:
#include "ui_manager.h"      // 提供 extern bool inCustomColorMode; 和 void drawDebugInfo();


// --- 由 power_manager 管理的全局状态变量 ---
// 这些将在 power_manager.cpp 中定义
extern bool isScreenOn;                 // 当前屏幕是否亮起
extern bool hasNewUpdateWhileScreenOff; // 屏幕关闭期间是否有新的绘图更新 (用于呼吸灯)

// --- Function Declarations ---

// 初始化电源管理相关引脚和状态
void powerManagerInit();

// 更新呼吸灯状态 (通常在屏幕关闭且有更新时调用)
void updateBreathLED();

// 读取电池电压并转换为百分比
float readBatteryVoltagePercentage();

// 处理 BOOT (IO0) 按钮的短按和长按逻辑 (屏幕开关、深度睡眠)
void handleBootButton();

// 管理屏幕关闭时的 LED 指示灯状态
void manageScreenStateLEDs();

// 切换屏幕状态 (亮/灭) - 辅助函数，可能被 handleBootButton 调用
void toggleScreen();

#endif // POWER_MANAGER_H

#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H
#include <TFT_eSPI.h>
#include "config.h"
#include <XPT2046_Touchscreen.h> // For TS_Point and XPT2046_Touchscreen object

// 原始触摸数据结构
typedef struct XY_structure_s // 已重命名以避免与原始 XY_structure (如果仍在某处) 冲突
{
    float x;          // 原始触摸 X 坐标
    float y;          // 原始触摸 Y 坐标
    bool fly = false; // 是否为无效触摸点 (飞点)
} XY_TouchPoint_t;    // 重命名的 typedef

// --- Extern 全局变量 ---
// ts 对象定义于 Project-ESPNow.ino
extern XPT2046_Touchscreen ts; 
// tft 对象也由 handleLocalTouch 的绘图部分需要，定义于 Project-ESPNow.ino
extern TFT_eSPI tft; 
// mySpi 对象定义于 Project-ESPNow.ino
extern SPIClass mySpi;


// --- 函数声明 ---

// SPI总线切换函数
void initTouchSPI(); // 初始化SPI为触摸屏配置
void initSDSPI();    // 初始化SPI为SD卡配置

// 初始化触摸处理 (如果需要特定的设置)
void touchHandlerInit();

// 处理本地触摸输入
// 此函数对 ui_manager 和 esp_now_handler 有许多依赖
// 这些依赖将在 touch_handler.cpp 中包含
void handleLocalTouch();

// 触摸点平均值计算 (内部辅助函数，也可以不在此声明如果只在 .cpp 中使用)
XY_TouchPoint_t averageXY(); 

#endif // TOUCH_HANDLER_H

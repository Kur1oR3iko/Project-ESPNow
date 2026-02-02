#ifndef CONFIG_H
#define CONFIG_H

// 触摸屏引脚定义
#define XPT2046_IRQ 36     // XPT2046 中断引脚
#define XPT2046_MOSI 32    // XPT2046 SPI MOSI 引脚
#define XPT2046_MISO 39    // XPT2046 SPI MISO 引脚
#define XPT2046_CLK 25     // XPT2046 SPI 时钟引脚
#define XPT2046_CS 33      // XPT2046 SPI 片选引脚

// IO0 按钮引脚定义
#define BUTTON_IO0 0       // GPIO 0, 通常是 Boot 按钮

// LED 及背光引脚定义
#define TFT_BL 21          // GPIO 21 用于控制 TFT 背光
#define GREEN_LED 16       // GPIO 16 用于控制绿色 LED
#define BLUE_LED 17        // GPIO 17 用于控制蓝色 LED
#define RED_LED 22         // GPIO 22 用于控制红色 LED

// 电池检测引脚定义
#define BATTERY_PIN 34     // GPIO 34, 用于读取电池电压

// TF卡引脚定义
#define SD_CS 5           // TF卡片选信号
#define SD_MOSI 23       // TF卡SPI总线写数据信号（MicroSD 卡和SPI外设共用）
#define SD_MISO 19        // TF卡SPI总线读数据信号（MicroSD 卡和SPI外设共用）
#define SD_SCK 18         // TF卡SPI总线时钟信号（MicroSD 卡和SPI外设共用）

// 触摸校准参数
#define TOUCH_MIN_X 200    // 触摸 X 轴最小值
#define TOUCH_MAX_X 3700   // 触摸 X 轴最大值
#define TOUCH_MIN_Y 300    // 触摸 Y 轴最小值
#define TOUCH_MAX_Y 3800   // 触摸 Y 轴最大值

// 重置按钮位置和大小
#define RESET_BUTTON_X 4   // 重置按钮 X 坐标
#define RESET_BUTTON_Y 4   // 重置按钮 Y 坐标 (调整为 4)
#define RESET_BUTTON_W 28  // 重置按钮宽度 (收窄)
#define RESET_BUTTON_H 10  // 重置按钮高度

// 颜色按钮位置和大小 (收窄并靠左)
#define COLOR_BUTTON_WIDTH 15                                      // 颜色按钮宽度 (收窄)
#define COLOR_BUTTON_HEIGHT 10                                     // 颜色按钮高度
#define COLOR_BUTTON_START_Y (RESET_BUTTON_Y + RESET_BUTTON_H + 2) // 颜色按钮起始 Y 坐标 (更紧凑)
#define COLOR_BUTTON_SPACING 2                                     // 颜色按钮间距

// 橡皮擦按钮位置和大小 (圆形)
#define ERASER_BUTTON_X (RESET_BUTTON_X + COLOR_BUTTON_WIDTH / 2) // 橡皮擦按钮中心 X 坐标 (与颜色按钮对齐)
#define ERASER_BUTTON_Y (COLOR_BUTTON_START_Y + (COLOR_BUTTON_HEIGHT + COLOR_BUTTON_SPACING) * 4 + ERASER_BUTTON_RADIUS) // 橡皮擦按钮中心 Y 坐标 (在所有颜色按钮下方，上移2像素)
#define ERASER_BUTTON_RADIUS 7                                     // 橡皮擦按钮半径

// 对端信息按钮位置和大小
#define PEER_INFO_BUTTON_X RESET_BUTTON_X                                                    // 对端信息按钮 X 坐标 (与重置按钮对齐)
#define PEER_INFO_BUTTON_Y (ERASER_BUTTON_Y + ERASER_BUTTON_RADIUS + 3) // 对端信息按钮 Y 坐标 (在橡皮擦按钮下方，下移1像素)
#define PEER_INFO_BUTTON_W 10                                                                        // 对端信息按钮宽度
#define PEER_INFO_BUTTON_H 10                                                                        // 对端信息按钮高度

// 截屏按钮位置和大小
#define SCREENSHOT_BUTTON_X (SCREEN_WIDTH - COLOR_BUTTON_WIDTH) // 截屏按钮 X 坐标 (屏幕右侧靠边)
#define SCREENSHOT_BUTTON_Y (SCREEN_HEIGHT - COLOR_BUTTON_HEIGHT - 2) // 截屏按钮 Y 坐标 (更靠下)
#define SCREENSHOT_BUTTON_W COLOR_BUTTON_WIDTH                      // 截屏按钮宽度
#define SCREENSHOT_BUTTON_H COLOR_BUTTON_HEIGHT                     // 截屏按钮高度

// 橡皮擦相关常量
#define ERASER_COLOR TFT_WHITE      // 橡皮擦颜色 (白色)
#define ERASER_ACTIVE_COLOR TFT_CYAN // 橡皮擦激活时颜色 (青色)
#define ERASER_MIN_RADIUS 1        // 橡皮擦最小半径
#define ERASER_MAX_RADIUS 20       // 橡皮擦最大半径

// 橡皮擦滑块配置
#define ERASER_SLIDER_X (ERASER_BUTTON_X + ERASER_BUTTON_RADIUS + 5) // 滑块 X 坐标 (橡皮擦按钮右侧)
#define ERASER_SLIDER_Y ERASER_BUTTON_Y // 滑块 Y 坐标 (与橡皮擦按钮对齐)
#define ERASER_SLIDER_WIDTH 6       // 滑块宽度
#define ERASER_SLIDER_HEIGHT 60      // 滑块高度
#define ERASER_SLIDER_HANDLE_W 10     // 滑块手柄宽度
#define ERASER_SLIDER_HANDLE_H 8      // 滑块手柄高度

// 对端信息界面相关常量
#define MAX_PEERS_TO_DISPLAY 8 // 对端信息界面最多显示的对端数量
#define PEER_INFO_UPDATE_INTERVAL 500UL // 对端信息界面更新间隔 (毫秒)

// 自定义颜色按钮 ("*") 位置和大小
#define CUSTOM_COLOR_BUTTON_X (SCREEN_WIDTH - COLOR_BUTTON_WIDTH - 4) // 自定义颜色按钮 X 坐标 (屏幕右侧)
#define CUSTOM_COLOR_BUTTON_Y 4                                       // 自定义颜色按钮 Y 坐标
#define CUSTOM_COLOR_BUTTON_W COLOR_BUTTON_WIDTH                      // 自定义颜色按钮宽度 (与普通颜色按钮相同)
#define CUSTOM_COLOR_BUTTON_H COLOR_BUTTON_HEIGHT                     // 自定义颜色按钮高度 (与普通颜色按钮相同)

// 返回按钮 (调色界面中使用) 位置和大小
#define BACK_BUTTON_X (SCREEN_WIDTH - COLOR_BUTTON_WIDTH - 4)     // 返回按钮 X 坐标 (屏幕右侧)
#define BACK_BUTTON_Y (SCREEN_HEIGHT - COLOR_BUTTON_HEIGHT - 4) // 返回按钮 Y 坐标 (屏幕右下角)
#define BACK_BUTTON_W COLOR_BUTTON_WIDTH                          // 返回按钮宽度
#define BACK_BUTTON_H COLOR_BUTTON_HEIGHT                         // 返回按钮高度

// 屏幕宽高定义
#define SCREEN_WIDTH 320  // 屏幕宽度 (像素)
#define SCREEN_HEIGHT 240 // 屏幕高度 (像素)

// 手动定义 TFT_GRAY (灰色)
// 标准 TFT_eSPI 库通常包含此颜色，但为确保可用性在此处定义
// 格式为 RGB565, 计算方式: ( (R & 0xF8) << 8 ) | ( (G & 0xFC) << 3 ) | ( B >> 3 )
// 对于灰色 (128, 128, 128): R=128 (0x80), G=128 (0x80), B=128 (0x80)
// (0x80 & 0xF8) << 8  => 0x8000
// (0x80 & 0xFC) << 3  => 0x0400 (0x80 >> 2 << 5)
// (0x80 >> 3)         => 0x0010
// 0x8000 | 0x0400 | 0x0010 = 0x8410
#define TFT_GRAY 0x8410

// ESP-NOW 通信相关常量
#define BROADCAST_INTERVAL 2000             // MAC 地址发现广播间隔 (毫秒)
#define UPTIME_INFO_BROADCAST_INTERVAL 5000 // Uptime 信息广播间隔 (毫秒)

// UI 更新相关常量
#define DEBUG_INFO_UPDATE_INTERVAL 200      // 调试信息更新间隔 (毫秒)

// 调色界面相关常量
#define COLOR_SLIDER_WIDTH 20                               // 颜色滑块宽度
#define COLOR_SLIDER_HEIGHT ((SCREEN_HEIGHT - 10) / 3)      // 单个颜色滑块高度

// LED 调光相关常量 (息屏状态下)
#define BLUE_LED_DIM_DUTY_CYCLE 14 // 蓝色 LED 息屏时亮度 (PWM duty cycle, 0-255), 约 5% (14/255)
#define RED_LED_DIM_DUTY_CYCLE 4   // 红色 LED 息屏时亮度 (PWM duty cycle, 0-255), 约 1.5% (4/255)

// 触摸笔划间隔阈值 (毫秒)
// 小于此间隔的触摸/绘制事件被视为连续笔划的一部分
#define TOUCH_STROKE_INTERVAL 50

// ESP-NOW 同步逻辑相关常量
#define MIN_UPTIME_DIFF_FOR_NEW_SYNC_TARGET 200UL // 选择新的同步目标时，对端设备最小原始运行时间差异 (毫秒) - 用于迟滞判断
#define EFFECTIVE_UPTIME_SYNC_THRESHOLD 1000UL    // 有效运行时间同步阈值 (毫秒) - 在此阈值内的差异不触发新的同步以避免抖动

// 心跳包相关常量
#define HEARTBEAT_SEND_INTERVAL_MS 5000UL // 心跳包发送间隔 (毫秒)
#define HEARTBEAT_TIMEOUT_MS 10000UL      // 心跳超时时间 (毫秒)，10秒

// 调试信息切换按钮位置和大小
#define DEBUG_TOGGLE_BUTTON_X 2                     // 按钮 X 坐标 (左下角)
#define DEBUG_TOGGLE_BUTTON_W 20                    // 按钮宽度
#define DEBUG_TOGGLE_BUTTON_H 20                    // 按钮高度
#define DEBUG_TOGGLE_BUTTON_Y (SCREEN_HEIGHT - DEBUG_TOGGLE_BUTTON_H - 2) // 按钮 Y 坐标

// 圆形进度条相关定义
#define PROGRESS_CIRCLE_RADIUS 8
#define PROGRESS_CIRCLE_THICKNESS 3
#define SEND_PROGRESS_X (RESET_BUTTON_X + PROGRESS_CIRCLE_RADIUS) // 与重置按钮左对齐，并考虑半径
#define SEND_PROGRESS_Y (PEER_INFO_BUTTON_Y + PEER_INFO_BUTTON_H + PROGRESS_CIRCLE_RADIUS + 5) // 对端信息按钮下方
#define RECEIVE_PROGRESS_X SEND_PROGRESS_X
#define RECEIVE_PROGRESS_Y (SEND_PROGRESS_Y + 2 * PROGRESS_CIRCLE_RADIUS + 5) // 发送进度条下方
#define PROGRESS_SEND_COLOR TFT_BLUE
#define PROGRESS_RECEIVE_COLOR TFT_GREEN
#define PROGRESS_BG_COLOR TFT_DARKGREY

// 项目信息按钮 (调试界面上方)
#define INFO_BUTTON_W 15
#define INFO_BUTTON_H 15
#define INFO_BUTTON_X (2 + 120 - INFO_BUTTON_W - 2) // 调试信息框 (startX=2, width=120) 右上角
#define INFO_BUTTON_Y (SCREEN_HEIGHT - 42 - INFO_BUTTON_H - 2) // 调试信息框 (startY=SCREEN_HEIGHT-42) 上方

// "Coffee" 按钮 (调试按钮上方)
#define COFFEE_BUTTON_X DEBUG_TOGGLE_BUTTON_X      // 与调试按钮 X 坐标相同
#define COFFEE_BUTTON_W DEBUG_TOGGLE_BUTTON_W      // 与调试按钮宽度相同
#define COFFEE_BUTTON_H COFFEE_BUTTON_W      // 与调试按钮高度相同
#define COFFEE_BUTTON_Y (DEBUG_TOGGLE_BUTTON_Y - COFFEE_BUTTON_H - 2) // 在调试按钮上方，间隔2像素

#endif // CONFIG_H

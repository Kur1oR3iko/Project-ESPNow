#include "touch_handler.h"
#include "config.h"
#include <Arduino.h> // 用于 millis, delay, Serial, map 等

// 包含依赖模块的头文件
#include "ui_manager.h"       // 用于UI函数和状态 (inCustomColorMode, currentColor, currentUIState 等)
#include "esp_now_handler.h"  // 用于ESP-NOW函数 (sendSyncMessage) 和数据 (allDrawingHistory 等)
#include "drawing_history.h" // 包含自定义绘图历史头文件

// --- 静态 (文件局部) 全局变量，用于触摸处理状态 ---
static TS_Point lastLocalPoint = {0, 0, 0};  // 本地最后一次触摸点坐标
static unsigned long lastLocalTouchTime = 0; // 本地最后一次触摸事件的时间戳

// 来自 ui_manager 的外部变量
extern bool isEraserMode; // 橡皮擦模式状态
extern int eraserRadius; // 当前橡皮擦半径
extern bool isEraserSliderVisible; // 橡皮擦滑块是否可见

// 来自 Project-ESPNow.ino 的外部变量
extern SPIClass mySpi; // SPI对象

// 彩蛋相关变量，现为本模块局部变量
static unsigned long lastResetTime = 0;
static int resetPressCount = 0;

// 橡皮擦按钮防抖
static unsigned long lastEraserButtonTime = 0;
#define ERASER_BUTTON_DEBOUNCE_TIME 300 // 橡皮擦按钮防抖时间（毫秒）

// 按钮区域边界（用于避免橡皮擦涂黑按钮）
#define BUTTON_AREA_X_MAX (ERASER_SLIDER_X + ERASER_SLIDER_WIDTH + ERASER_SLIDER_HANDLE_W + 10) // 左侧按钮区域最大 X 坐标（包括滑块）
#define BUTTON_AREA_Y_MAX (PEER_INFO_BUTTON_Y + PEER_INFO_BUTTON_H + 10) // 按钮区域最大 Y 坐标
#define BUTTON_AREA_RIGHT_X_MIN (SCREEN_WIDTH - COLOR_BUTTON_WIDTH - 4) // 右侧按钮区域最小 X 坐标（自定义颜色按钮）
#define BUTTON_AREA_RIGHT_Y_MIN 4 // 右侧按钮区域最小 Y 坐标
#define BUTTON_AREA_RIGHT_Y_MAX (SCREEN_HEIGHT - 4) // 右侧按钮区域最大 Y 坐标（包括返回按钮）

// --- 函数实现 ---

// SPI总线切换函数 - 初始化SPI为触摸屏配置
void initTouchSPI() {
    mySpi.end();
    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    mySpi.setFrequency(2500000);
    mySpi.setDataMode(SPI_MODE0);
    mySpi.setBitOrder(MSBFIRST);
    ts.begin(mySpi);
    ts.setRotation(1);
}

// SPI总线切换函数 - 初始化SPI为SD卡配置
void initSDSPI() {
    mySpi.end();
    mySpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    mySpi.setFrequency(40000000);
    mySpi.setDataMode(SPI_MODE0);
    mySpi.setBitOrder(MSBFIRST);
}

void touchHandlerInit() {
    // 如果将来需要任何触摸相关的特定初始化，则为占位符
    // 例如：ts.setThreshold(某个值);
}

// 触摸点平均值计算
XY_TouchPoint_t averageXY() { // 返回类型已更改为 XY_TouchPoint_t
    // TS_Point p = ts.getPoint(); // 初始的 getPoint 由于循环似乎是多余的
    bool fly = false;
    int cnt = 0;
    int i, j, k, min_idx, temp_val; // 将 min 重命名为 min_idx，temp 重命名为 temp_val，以避免与潜在函数冲突
    int tmp[2][10]; // 10个样本的缓冲区

    for (cnt = 0; cnt < 10; cnt++) { // 收集10个样本
        TS_Point p_sample = ts.getPoint();
        if (p_sample.z > 200) { // 检查压力阈值
            tmp[0][cnt] = p_sample.x;
            tmp[1][cnt] = p_sample.y;
            delay(2); // 样本之间的小延迟
        } else {
            fly = true; // 无效触摸 (太轻或无触摸)
            break;
        }
    }

    XY_TouchPoint_t resultXY; // 已更改类型
    if (fly || cnt < 4) { // 如果标记为飞点或有效样本不足 (例如，平均中间4个点至少需要4个)
        resultXY.fly = true;
        return resultXY;
    }

    // 对样本进行排序以移除异常值 (对于小N使用简单冒泡排序)
    for (k = 0; k < 2; k++) { // 对于 X (0) 和 Y (1)
        for (i = 0; i < cnt - 1; i++) {
            min_idx = i;
            for (j = i + 1; j < cnt; j++) {
                if (tmp[k][min_idx] > tmp[k][j]) {
                    min_idx = j;
                }
            }
            // 交换
            temp_val = tmp[k][i];
            tmp[k][i] = tmp[k][min_idx];
            tmp[k][min_idx] = temp_val;
        }
    }

    // 平均中间样本
    // 对于不同的cnt值，选择合适的中间样本
    if (cnt >= 10) {
        // 如果有10个样本，平均中间4个 (索引3,4,5,6)
        resultXY.x = (tmp[0][3] + tmp[0][4] + tmp[0][5] + tmp[0][6]) / 4.0f;
        resultXY.y = (tmp[1][3] + tmp[1][4] + tmp[1][5] + tmp[1][6]) / 4.0f;
        resultXY.fly = false;
    } else if (cnt >= 7) {
        // 如果有7-9个样本，平均中间3个
        int mid = cnt / 2;
        resultXY.x = (tmp[0][mid-1] + tmp[0][mid] + tmp[0][mid+1]) / 3.0f;
        resultXY.y = (tmp[1][mid-1] + tmp[1][mid] + tmp[1][mid+1]) / 3.0f;
        resultXY.fly = false;
    } else if (cnt >= 4) {
        // 如果有4-6个样本，平均所有样本
        int sumX = 0, sumY = 0;
        for (int i = 0; i < cnt; i++) {
            sumX += tmp[0][i];
            sumY += tmp[1][i];
        }
        resultXY.x = sumX / (float)cnt;
        resultXY.y = sumY / (float)cnt;
        resultXY.fly = false;
    } else { // 点数不足
        resultXY.fly = true;
    }
    return resultXY;
}


// 处理本地触摸输入
void handleLocalTouch() {
    // 来自 ui_manager.h 的依赖项:
    // extern bool inCustomColorMode;
    // extern uint32_t currentColor;
    // extern UIState_t currentUIState; // 新增
    // ... (其他UI相关函数)

    // 来自 esp_now_handler.h 的依赖项:
    extern DrawingHistory allDrawingHistory;
    // extern long relativeBootTimeOffset;
    // ... (其他ESP-NOW相关函数和变量)

    // tft 和 ts 是来自 touch_handler.h 的 extern 变量 (在 .ino 中定义)

    float x1, y1;
    bool touched = ts.tirqTouched() && ts.touched(); // 检查IRQ，然后通过压力确认
    unsigned long currentRawUptime = millis();       // 此触摸事件的本地原始运行时间
    XY_TouchPoint_t xy1;

    if (touched) {
        // 首先处理弹窗关闭逻辑 (Coffee 弹窗优先于项目信息弹窗，如果两者都可能存在)
        if (isCoffeePopupVisible) {
            xy1 = averageXY(); // 确认是有效触摸
            if (!xy1.fly) {
                hideCoffeePopup(); // 关闭 Coffee 弹窗
                lastLocalPoint.z = 0; // 标记为无触摸
                return; // 操作已处理
            }
        } else if (isProjectInfoPopupVisible) { // 然后处理项目信息弹窗的关闭逻辑
            xy1 = averageXY();
            if (!xy1.fly) {
                 hideProjectInfoPopup(); // 关闭弹窗
                 lastLocalPoint.z = 0; // 标记为无触摸, 避免后续处理
                 return; // 操作已处理
            }
        }

        xy1 = averageXY(); // 获取滤波后的触摸点
        x1 = xy1.x;
        y1 = xy1.y;

        if (!xy1.fly) { // 如果触摸点有效
            // 将原始触摸坐标映射到屏幕坐标
            int mapX = map(x1, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
            int mapY = map(y1, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);

            // 根据当前 UI 状态处理触摸事件
            switch (currentUIState) {
                case UI_STATE_MAIN: // 主绘图界面
                    if (inCustomColorMode) { // 如果处于自定义颜色选择模式
                        handleCustomColorTouch(mapX, mapY); // 传递给UI管理器的自定义颜色处理函数
                    } else { // 正常绘图/按钮模式
                        // 检查 "Coffee" 按钮
                        if (showDebugToggleButton && isCoffeeButtonPressed(mapX, mapY)) { // D按钮显示时C按钮才有效
                            showCoffeePopup();
                            return; // 操作已处理
                        }

                        // 检查项目信息按钮
                        if (isDebugInfoVisible && isInfoButtonPressed(mapX, mapY)) {
                            showProjectInfoPopup();
                            return; // 操作已处理
                        }

                        // 首先检查调试相关按钮的点击
                        // 注意：Coffee 按钮的检查已在其之前
                        if (isDebugInfoVisible) {
                            // 点击调试信息框本身来切换 (InfoButton 和 CoffeeButton 已被优先处理)
                            if (mapX >= 2 && mapX <= 122 && mapY >= (SCREEN_HEIGHT - 42) && mapY <= SCREEN_HEIGHT) {
                                if (!isInfoButtonPressed(mapX, mapY)) { // 确保不是误点InfoButton区域
                                     toggleDebugInfo();
                                     return; // 操作已处理
                                }
                            }
                        } else if (showDebugToggleButton) { // 如果调试切换按钮可见 (D按钮)
                            if (isDebugToggleButtonPressed(mapX, mapY)) {
                                toggleDebugInfo();
                                return; // 操作已处理
                            }
                        }

                        // 检查复位按钮是否按下
                        if (isResetButtonPressed(mapX, mapY)) {
                            if (currentRawUptime - lastResetTime < 1000) { // 检查快速按下 (彩蛋)
                                resetPressCount++;
                            } else {
                                resetPressCount = 1;
                            }
                            lastResetTime = currentRawUptime;

                            if (resetPressCount >= 10) { // 彩蛋触发
                                Serial.println("Kurio Reiko thanks all the recognition and redistribution,");
                                Serial.println("but if someone commercializes this project without declaring Kurio Reiko's originality, then he is a bitch");
                                resetPressCount = 0; // 重置计数器
                            }

                            // 清除本地绘图历史 (allDrawingHistory 是来自 esp_now_handler 的 extern 变量)
                            allDrawingHistory.clear();
                            clearScreenAndCache();     // 调用UI管理器的清屏函数

                            // 重置ESP-NOW同步状态变量 (来自 esp_now_handler 的 extern 变量)
                            // 这部分逻辑可能更适合放在 esp_now_handler 内部，
                            // 由 "local_reset" 事件/标志触发。
                            relativeBootTimeOffset = 0;
                            iamEffectivelyMoreUptimeDevice = false; // 这些现在位于 esp_now_handler 中
                            iamRequestingAllData = false;
                            initialSyncLogicProcessed = false;

                            // 通过ESP-NOW发送复位消息
                            SyncMessage_t resetMsg;
                            resetMsg.type = MSG_TYPE_RESET_CANVAS;
                            resetMsg.senderUptime = currentRawUptime;
                            resetMsg.senderOffset = relativeBootTimeOffset; // 现在应为0
                            resetMsg.touch_data.isReset = true;
                            resetMsg.touch_data.timestamp = currentRawUptime;
                            resetMsg.touch_data.x = 0; // 与复位无关
                            resetMsg.touch_data.y = 0; // 与复位无关
                            resetMsg.touch_data.color = currentColor; // currentColor 来自 ui_manager
                            sendSyncMessage(&resetMsg); // sendSyncMessage 来自 esp_now_handler

                            // 本地绘图历史 (allDrawingHistory) 已被清除。
                            // 屏幕和UI缓存 (clearScreenAndCache()) 已被清除。
                            // 复位消息已发送通知对端设备进行清除。
                            // 根据新的逻辑，不再将此复位操作作为点位记录到本地历史中。
                            return; // 操作已处理
                        }

                        uint32_t selectedColorHolder; // 颜色按钮的临时占位符
                        if (isColorButtonPressed(mapX, mapY, selectedColorHolder)) { // 检查标准颜色按钮
                            updateCurrentColor(selectedColorHolder); // 在 ui_manager 中更新颜色
                            redrawStarButton();                      // 用新颜色重绘星星按钮 (ui_manager)
                            isEraserMode = false; // 退出橡皮擦模式
                            redrawEraserButton(); // 重绘橡皮擦按钮
                            return; // 操作已处理
                        }

                        // 检查橡皮擦按钮
                        if (isEraserButtonPressed(mapX, mapY)) {
                            // 防抖：确保距离上次点击至少300毫秒
                            if (currentRawUptime - lastEraserButtonTime >= ERASER_BUTTON_DEBOUNCE_TIME) {
                                isEraserMode = !isEraserMode; // 切换橡皮擦模式
                                isEraserSliderVisible = isEraserMode; // 显示/隐藏滑块
                                lastEraserButtonTime = currentRawUptime; // 更新最后点击时间
                                if (!isEraserSliderVisible) {
                                    // 隐藏滑块时，重绘整个屏幕以清除滑块
                                    redrawMainScreen();
                                } else {
                                    // 显示滑块时，只重绘橡皮擦按钮和滑块
                                    redrawEraserButton();
                                    drawEraserSlider();
                                }
                            }
                            return; // 操作已处理
                        }

                        // 检查橡皮擦滑块
                        if (isEraserSliderPressed(mapX, mapY)) {
                            handleEraserSliderTouch(mapX, mapY); // 处理滑块触摸
                            return; // 操作已处理
                        }

                        // 检查对端信息按钮
                        if (isPeerInfoButtonPressed(mapX, mapY)) {
                            showPeerInfoScreen(); // 切换到对端信息界面
                            return; // 操作已处理
                        }

                        // 检查休眠按钮 (已改为对端信息按钮，此处的逻辑应移除或修改)
                        // 根据之前的修改，这个按钮现在是对端信息按钮，上面的 if 已经处理了。
                        // 如果需要一个单独的休眠按钮，需要重新添加UI元素和逻辑。
                        // 暂时移除此处的 isSleepButtonPressed 检查
                        /*
                        if (isSleepButtonPressed(mapX, mapY)) { // 检查休眠按钮
                            Serial.println("准备进入深度睡眠模式...");
                            esp_deep_sleep_start(); // ESP32 API 用于深度睡眠
                            return; // 操作已处理 (尽管设备将休眠)
                        }
                        */

                        if (isCustomColorButtonPressed(mapX, mapY)) { // 检查自定义颜色 ("*") 按钮
                            inCustomColorMode = true;   // 设置UI状态 (ui_manager)
                            saveScreenArea();           // 保存屏幕区域 (ui_manager)
                            drawColorSelectors();       // 绘制颜色选择器 (ui_manager)
                            hideStarButton();           // 隐藏星星按钮 (ui_manager)
                            return; // 操作已处理
                        }

                        // 检查截屏按钮
                        if (isScreenshotButtonPressed(mapX, mapY)) {
                            saveScreenshotToSD(); // 保存截屏到SD卡
                            return; // 操作已处理
                        }

                        // 如果橡皮擦滑块可见且点击了空白区域，则隐藏滑块并执行橡皮擦操作
                        if (isEraserSliderVisible) {
                            isEraserSliderVisible = false;
                            redrawMainScreen(); // 重绘整个屏幕以清除滑块
                            // 继续执行橡皮擦操作
                        }

                        // 如果没有按钮被按下，则继续执行绘图逻辑
                        uint32_t drawColor = isEraserMode ? TFT_BLACK : currentColor; // 橡皮擦模式使用黑色，否则使用当前颜色
                        if (isEraserMode) {
                            // 橡皮擦模式：绘制可变半径的圆形
                            // 检查橡皮擦圆是否会与按钮区域相交
                            // 左侧按钮区域：x从0到BUTTON_AREA_X_MAX，y从0到BUTTON_AREA_Y_MAX
                            // 右侧按钮区域：x从BUTTON_AREA_RIGHT_X_MIN到SCREEN_WIDTH，y从BUTTON_AREA_RIGHT_Y_MIN到BUTTON_AREA_RIGHT_Y_MAX
                            
                            // 计算圆心到左侧按钮区域的最短距离
                            int dx_left = 0;
                            int dy_left = 0;
                            
                            if (mapX < 0) {
                                dx_left = -mapX;
                            } else if (mapX > BUTTON_AREA_X_MAX) {
                                dx_left = mapX - BUTTON_AREA_X_MAX;
                            }
                            
                            if (mapY < 0) {
                                dy_left = -mapY;
                            } else if (mapY > BUTTON_AREA_Y_MAX) {
                                dy_left = mapY - BUTTON_AREA_Y_MAX;
                            }
                            
                            // 计算圆心到右侧按钮区域的最短距离
                            int dx_right = 0;
                            int dy_right = 0;
                            
                            if (mapX < BUTTON_AREA_RIGHT_X_MIN) {
                                dx_right = BUTTON_AREA_RIGHT_X_MIN - mapX;
                            } else if (mapX > SCREEN_WIDTH) {
                                dx_right = mapX - SCREEN_WIDTH;
                            }
                            
                            if (mapY < BUTTON_AREA_RIGHT_Y_MIN) {
                                dy_right = BUTTON_AREA_RIGHT_Y_MIN - mapY;
                            } else if (mapY > BUTTON_AREA_RIGHT_Y_MAX) {
                                dy_right = mapY - BUTTON_AREA_RIGHT_Y_MAX;
                            }
                            
                            // 如果圆心到左侧按钮区域的距离小于半径，则橡皮擦会覆盖左侧按钮
                            bool hitLeftButton = (dx_left * dx_left + dy_left * dy_left < eraserRadius * eraserRadius);
                            // 如果圆心到右侧按钮区域的距离小于半径，则橡皮擦会覆盖右侧按钮
                            bool hitRightButton = (dx_right * dx_right + dy_right * dy_right < eraserRadius * eraserRadius);
                            
                            // 只有当橡皮擦不会覆盖任何按钮时才执行擦除
                            if (!hitLeftButton && !hitRightButton) {
                                tft.fillCircle(mapX, mapY, eraserRadius, TFT_BLACK);
                            }
                        } else {
                            // 普通绘图模式
                            if (currentRawUptime - lastLocalTouchTime > TOUCH_STROKE_INTERVAL || lastLocalPoint.z == 0) {
                                // 新的笔划或抬起后的第一个点
                                tft.drawPixel(mapX, mapY, drawColor); // 使用计算出的颜色
                            } else {
                                // 继续现有笔划
                                tft.drawLine(lastLocalPoint.x, lastLocalPoint.y, mapX, mapY, drawColor); // 使用计算出的颜色
                            }
                        }

                        // 更新最后本地触摸点状态
                        lastLocalPoint = {mapX, mapY, 1}; // 存储映射后的坐标
                        lastLocalTouchTime = currentRawUptime;

                        // 创建用于历史记录和发送的触摸数据
                        TouchData_t currentDrawPoint;
                        currentDrawPoint.x = mapX;
                        currentDrawPoint.y = mapY;
                        currentDrawPoint.timestamp = currentRawUptime;
                        currentDrawPoint.isReset = false;
                        currentDrawPoint.color = drawColor; // 使用计算出的颜色

                        allDrawingHistory.push_back(currentDrawPoint); // 添加到本地历史 (esp_now_handler 的 extern 变量)

                        // 准备并通过ESP-NOW发送绘图数据
                        SyncMessage_t drawMsg;
                        drawMsg.type = MSG_TYPE_DRAW_POINT;
                        drawMsg.senderUptime = currentRawUptime;
                        drawMsg.senderOffset = relativeBootTimeOffset; // relativeBootTimeOffset 来自 esp_now_handler
                        drawMsg.touch_data = currentDrawPoint;
                        sendSyncMessage(&drawMsg); // sendSyncMessage 来自 esp_now_handler
                    }
                    break; // End of UI_STATE_MAIN case

                case UI_STATE_COLOR_PICKER: // 颜色选择器界面
                    handleCustomColorTouch(mapX, mapY); // 触摸处理已在 ui_manager 中实现
                    break; // End of UI_STATE_COLOR_PICKER case

                case UI_STATE_POPUP: // 弹窗界面 (项目信息或 Coffee)
                    // 弹窗触摸处理已在 handleLocalTouch 开头处理，这里无需额外逻辑
                    break; // End of UI_STATE_POPUP case

                case UI_STATE_PEER_INFO: // 对端信息界面
                    // 检查返回按钮
                    if (isPeerInfoScreenBackButtonPressed(mapX, mapY)) { // 更正为 isPeerInfoScreenBackButtonPressed
                        Serial.println("Peer Info Back button pressed. Hiding Peer Info Screen."); // 添加调试输出
                        hidePeerInfoScreen(); // 返回主界面
                        return; // 操作已处理
                    }
                    // 如果将来对端信息界面有其他可交互元素，可以在这里添加处理逻辑
                    break; // End of UI_STATE_PEER_INFO case

                default:
                    // 未知状态，可能需要默认行为或错误处理
                    break;
            }
        }
    } else { // 当前未检测到触摸
        lastLocalPoint.z = 0; // 标记为无触摸 (压力 = 0)
    }
}

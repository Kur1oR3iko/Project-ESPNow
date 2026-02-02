#include "drawing_history.h" // 包含自定义绘图历史头文件和 TouchData_t 的定义
#include "esp_now_handler.h"
#include "config.h"     // 包含项目配置常量
#include "ui_manager.h" // << 添加对 UI 管理器的引用
#include <Arduino.h>    // For Serial, millis, etc.
#include <cstring>      // For memcpy, memset, snprintf
#include <TFT_eSPI.h> // 需要 TFT_eSPI::color565 等，以及 tft 对象
#include "touch_handler.h" // For TS_Point type
#include <vector> // 用于 getPeerInfoList 返回值
#include <map> // 用于 std::map

// TFT_eSPI tft 对象和 drawMainInterface 函数在 Project-ESPNow.ino 中定义
// 通过 extern 声明来在此文件中使用它们
extern TFT_eSPI tft;
extern void drawMainInterface(); // 用于清屏后重绘UI骨架
// 如果 clearScreenAndCache 也需要从这里调用，也需要 extern
extern void clearScreenAndCache();

// 来自 ui_manager 的外部变量
extern int eraserRadius; // 当前橡皮擦半径

// 定义在 esp_now_handler.h 中声明的全局变量
esp_now_peer_info_t broadcastPeerInfo;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // ESP-NOW 广播地址
std::queue<SyncMessage_t> incomingMessageQueue;                    // ESP-NOW 接收消息队列
DrawingHistory allDrawingHistory;                        // 所有绘图操作的历史记录
std::set<String> macSet;                                           // 已发现的对端设备 MAC 地址
std::map<String, unsigned long> peerLastHeartbeat; // 存储每个对端的最后心跳时间

// PeerInfo_t 结构体定义已移至 esp_now_handler.h
// 已移除重复的 PeerInfo_s 结构体定义和 PeerInfo_t typedef

// 新增：存储所有已知对端详细信息的 map
std::map<String, PeerInfo_t> peerInfoMap;


unsigned long lastKnownPeerUptime = 0;
long lastKnownPeerOffset = 0;
uint8_t lastPeerMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool initialSyncLogicProcessed = false;
bool iamEffectivelyMoreUptimeDevice = false;
bool iamRequestingAllData = false;
bool isAwaitingSyncStartResponse = false;
bool isReceivingDrawingData = false; // 修正：这里之前少了一个 bool 关键字
bool isSendingDrawingData = false;
size_t currentHistorySendIndex = 0; // 定义新增的全局变量
long relativeBootTimeOffset = 0;
unsigned long uptimeOfLastPeerSyncedFrom = 0;
unsigned long timeRequestSentForAllDrawings = 0; // 新增：记录请求所有绘图数据的时间戳

// 用于接收进度条的变量
static size_t receivedHistoryPointCount = 0;
static uint16_t totalPointsExpectedFromPeer = 0;

// 触摸点处理相关 (用于远程点绘制)
TS_Point lastRemotePoint = {0, 0, 0}; // 远程最后一点
unsigned long lastRemoteDrawTime = 0; // 远程最后绘制时间
// unsigned long touchInterval = 50;     // 触摸笔划间隔阈值 (毫秒) -> 已移至 config.h 作为 TOUCH_STROKE_INTERVAL

// ESP-NOW 初始化函数
void espNowInit()
{
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("错误：ESP-NOW 初始化失败");
        return;
    }

    esp_now_register_send_cb(OnSyncDataSent);
    esp_now_register_recv_cb(OnSyncDataRecv);

    memcpy(broadcastPeerInfo.peer_addr, broadcastAddress, 6);
    broadcastPeerInfo.channel = 0;
    broadcastPeerInfo.ifidx = WIFI_IF_STA;
    broadcastPeerInfo.encrypt = false;
    if (esp_now_add_peer(&broadcastPeerInfo) != ESP_OK)
    {
        Serial.println("添加广播对端失败");
        esp_now_del_peer(broadcastPeerInfo.peer_addr); // 尝试删除后重新添加
        if (esp_now_add_peer(&broadcastPeerInfo) != ESP_OK)
        {
            Serial.println("尝试删除后重新添加广播对端仍然失败");
            return;
        }
        Serial.println("初次失败后成功重新添加广播对端。");
    }
    else
    {
        Serial.println("广播对端添加成功。");
    }
}

// ESP-NOW 数据发送回调函数
void OnSyncDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
    {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        Serial.print("发送到 ");
        Serial.print(macStr);
        Serial.print(" 失败。状态: ");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "成功" : "失败");
    }
}

// ESP-NOW 数据接收回调函数
void OnSyncDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataPtr, int len)
{
    if (len == sizeof(SyncMessage_t))
    {
        SyncMessage_t receivedMsg;
        memcpy(&receivedMsg, incomingDataPtr, sizeof(receivedMsg));
        memcpy(lastPeerMac, info->src_addr, 6); // 更新最后通信的对端 MAC

        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 info->src_addr[0], info->src_addr[1], info->src_addr[2],
                 info->src_addr[3], info->src_addr[4], info->src_addr[5]);
        macSet.insert(String(macStr)); // 添加到 MAC 地址集合中用于计数
        peerLastHeartbeat[String(macStr)] = millis(); // 更新对端的最后心跳时间

        // 更新或添加对端详细信息
        peerInfoMap[String(macStr)].macAddress = String(macStr);
        peerInfoMap[String(macStr)].effectiveUptime = receivedMsg.senderUptime + receivedMsg.senderOffset;
        peerInfoMap[String(macStr)].usedMemory = receivedMsg.usedMemory;
        peerInfoMap[String(macStr)].totalMemory = receivedMsg.totalMemory;


        incomingMessageQueue.push(receivedMsg); // 将消息放入队列等待处理
    }
    else if (len == strlen("XX:XX:XX:XX:XX:XX") && incomingDataPtr[0] != '{')
    {
        // 处理旧版或特定的 MAC 地址广播 (如果项目中有这种逻辑)
        char macStr[18];
        memcpy(macStr, incomingDataPtr, len);
        macStr[len] = '\0';
        macSet.insert(String(macStr));
        peerLastHeartbeat[String(macStr)] = millis(); // 更新对端的最后心跳时间
        // 对于旧版消息，我们没有内存信息，只更新心跳
    }
    else
    {
        Serial.print("收到意外长度的数据: ");
        Serial.print(len);
        Serial.print(", 期望长度: ");
        Serial.println(sizeof(SyncMessage_t));
    }
}

// 发送同步消息的辅助函数
void sendSyncMessage(const SyncMessage_t *msg)
{
    // 调用前应确保 msg->senderUptime 和 msg->senderOffset 已正确设置
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)msg, sizeof(SyncMessage_t));
    if (result != ESP_OK)
    {
        Serial.print("发送 SyncMessage 类型 ");
        Serial.print(msg->type);
        Serial.print(" 错误: ");
        Serial.println(esp_err_to_name(result));
    }
}

// 处理接收到的消息队列
void processIncomingMessages()
{
    // 声明外部变量/函数，如果它们在 .ino 或其他模块中定义
    extern bool isScreenOn;                 // 来自主 .ino 或 power_manager
    extern bool hasNewUpdateWhileScreenOff; // 来自主 .ino 或 power_manager

    while (!incomingMessageQueue.empty())
    {
        SyncMessage_t msg = incomingMessageQueue.front();
        incomingMessageQueue.pop();

        unsigned long localCurrentRawUptime = millis();
        long localCurrentOffset = relativeBootTimeOffset;
        unsigned long localEffectiveUptime = localCurrentRawUptime + localCurrentOffset;

        unsigned long peerRawUptime = msg.senderUptime;
        long peerReceivedOffset = msg.senderOffset;
        unsigned long peerEffectiveUptime = peerRawUptime + peerReceivedOffset;

        switch (msg.type)
        {
        case MSG_TYPE_UPTIME_INFO:
        {
            Serial.println("收到 MSG_TYPE_UPTIME_INFO");
            if (uptimeOfLastPeerSyncedFrom != 0 && !iamRequestingAllData)
            {
                unsigned long diffFromLastSyncSource = (peerRawUptime > uptimeOfLastPeerSyncedFrom) ? (peerRawUptime - uptimeOfLastPeerSyncedFrom) : (uptimeOfLastPeerSyncedFrom - peerRawUptime);
                if (diffFromLastSyncSource < MIN_UPTIME_DIFF_FOR_NEW_SYNC_TARGET)
                {
                    Serial.print("  迟滞判断: 对端原始运行时间 (");
                    Serial.print(peerRawUptime);
                    Serial.print(") 与上次同步源的原始运行时间 (");
                    Serial.print(uptimeOfLastPeerSyncedFrom);
                    Serial.print(") 差异 ");
                    Serial.print(diffFromLastSyncSource);
                    Serial.print("ms < ");
                    Serial.print(MIN_UPTIME_DIFF_FOR_NEW_SYNC_TARGET);
                    Serial.println("ms。跳过对此对端的完整同步评估。");
                    if (lastKnownPeerUptime != peerRawUptime || lastKnownPeerOffset != peerReceivedOffset)
                    {
                        lastKnownPeerUptime = peerRawUptime;
                        lastKnownPeerOffset = peerReceivedOffset;
                        initialSyncLogicProcessed = false;
                    }
                    if (!initialSyncLogicProcessed)
                        initialSyncLogicProcessed = true;
                    break;
                }
            }

            if (lastKnownPeerUptime != peerRawUptime || lastKnownPeerOffset != peerReceivedOffset || !initialSyncLogicProcessed)
            {
                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;

                Serial.println("  处理 UPTIME_INFO 进行同步决策。");
                Serial.print("  本地有效运行时间: ");
                Serial.print(localEffectiveUptime);
                Serial.print(" (原始: ");
                Serial.print(localCurrentRawUptime);
                Serial.print(", 偏移: ");
                Serial.print(localCurrentOffset);
                Serial.println(")");
                Serial.print("  对端有效运行时间: ");
                Serial.print(peerEffectiveUptime);
                Serial.print(" (原始: ");
                Serial.print(peerRawUptime);
                Serial.print(", 偏移: ");
                Serial.print(peerReceivedOffset);
                Serial.println(")");

                if (localEffectiveUptime == peerEffectiveUptime)
                {
                    uint8_t myMacAddr[6];
                    esp_wifi_get_mac(WIFI_IF_STA, myMacAddr);
                    if (memcmp(myMacAddr, lastPeerMac, 6) < 0)
                    {
                        Serial.println("  决策 (有效运行时间相同, MAC较小): 本机行为类似较新设备：清空并请求数据。");
                        if (iamRequestingAllData || isReceivingDrawingData || isSendingDrawingData)
                        {
                            Serial.println("  但当前已有同步正在进行，忽略新的同步请求发起。");
                            lastKnownPeerUptime = peerRawUptime; // 仍然更新对端信息
                            lastKnownPeerOffset = peerReceivedOffset;
                            initialSyncLogicProcessed = true; // 标记已处理此 UPTIME_INFO
                            break;
                        }
                        iamEffectivelyMoreUptimeDevice = false;
                        iamRequestingAllData = true;
                        isAwaitingSyncStartResponse = true; // 等待对方的 SYNC_START
                        allDrawingHistory.clear();
                        // tft.fillScreen(TFT_BLACK); // 清屏操作移至收到对方 SYNC_START 后
                        // drawMainInterface();

                        // 发送同步开始信号，表明本机准备好请求并接收数据
                        SyncMessage_t syncStartMsgBeforeRequest;
                        syncStartMsgBeforeRequest.type = MSG_TYPE_SYNC_START;
                        syncStartMsgBeforeRequest.senderUptime = localCurrentRawUptime;
                        syncStartMsgBeforeRequest.senderOffset = localCurrentOffset;
                        memset(&syncStartMsgBeforeRequest.touch_data, 0, sizeof(TouchData_t));
                        sendSyncMessage(&syncStartMsgBeforeRequest);
                        Serial.println("  发送 MSG_TYPE_SYNC_START (在请求所有绘图前)");

                        SyncMessage_t requestMsg;
                        requestMsg.type = MSG_TYPE_REQUEST_ALL_DRAWINGS;
                        requestMsg.senderUptime = localCurrentRawUptime;
                        requestMsg.senderOffset = localCurrentOffset;
                        memset(&requestMsg.touch_data, 0, sizeof(TouchData_t));
                        sendSyncMessage(&requestMsg);
                        timeRequestSentForAllDrawings = millis(); // 记录发送请求的时间
                    }
                    else
                    {
                        Serial.println("  决策 (有效运行时间相同, MAC较大/相等): 本机行为类似较旧设备：通知对端清空并请求数据。");
                        iamEffectivelyMoreUptimeDevice = true;
                        iamRequestingAllData = false;
                        SyncMessage_t promptMsg;
                        promptMsg.type = MSG_TYPE_CLEAR_AND_REQUEST_UPDATE;
                        promptMsg.senderUptime = localCurrentRawUptime;
                        promptMsg.senderOffset = localCurrentOffset;
                        memset(&promptMsg.touch_data, 0, sizeof(TouchData_t));
                        sendSyncMessage(&promptMsg);
                    }
                }
                else
                {
                    unsigned long effectiveUptimeDifference = (localEffectiveUptime > peerEffectiveUptime) ? (localEffectiveUptime - peerEffectiveUptime) : (peerEffectiveUptime - localEffectiveUptime);

                    if (effectiveUptimeDifference <= EFFECTIVE_UPTIME_SYNC_THRESHOLD)
                    {
                        Serial.println("  决策: 有效运行时间差在阈值内。不启动新的同步以避免抖动。");
                    }
                    else if (localEffectiveUptime < peerEffectiveUptime)
                    {
                        Serial.println("  决策: 本机有效运行时间较短 (超出阈值)。判定本机为较新设备。清空本机数据并向对端 (较旧设备) 请求所有绘图数据。");
                        if (iamRequestingAllData || isReceivingDrawingData || isSendingDrawingData)
                        {
                            Serial.println("  但当前已有同步正在进行，忽略新的同步请求发起。");
                            lastKnownPeerUptime = peerRawUptime; // 仍然更新对端信息
                            lastKnownPeerOffset = peerReceivedOffset;
                            initialSyncLogicProcessed = true; // 标记已处理此 UPTIME_INFO
                            break;
                        }
                        iamEffectivelyMoreUptimeDevice = false;
                        iamRequestingAllData = true;
                        isAwaitingSyncStartResponse = true; // 等待对方的 SYNC_START
                        allDrawingHistory.clear();
                        // tft.fillScreen(TFT_BLACK); // 清屏操作移至收到对方 SYNC_START 后
                        // drawMainInterface();

                        // 发送同步开始信号，表明本机准备好请求并接收数据
                        SyncMessage_t syncStartMsgBeforeRequest2;
                        syncStartMsgBeforeRequest2.type = MSG_TYPE_SYNC_START;
                        syncStartMsgBeforeRequest2.senderUptime = localCurrentRawUptime;
                        syncStartMsgBeforeRequest2.senderOffset = localCurrentOffset;
                        memset(&syncStartMsgBeforeRequest2.touch_data, 0, sizeof(TouchData_t));
                        sendSyncMessage(&syncStartMsgBeforeRequest2);
                        Serial.println("  发送 MSG_TYPE_SYNC_START (在请求所有绘图前 - UPTIME_INFO 路径)");

                        SyncMessage_t requestMsg;
                        requestMsg.type = MSG_TYPE_REQUEST_ALL_DRAWINGS;
                        requestMsg.senderUptime = localCurrentRawUptime;
                        requestMsg.senderOffset = localCurrentOffset;
                        memset(&requestMsg.touch_data, 0, sizeof(TouchData_t));
                        sendSyncMessage(&requestMsg);
                        timeRequestSentForAllDrawings = millis(); // 记录发送请求的时间
                    }
                    else
                    { // localEffectiveUptime > peerEffectiveUptime && diff > threshold
                        Serial.println("  决策: 本机有效运行时间较长 (超出阈值)。判定本机为较旧设备。通知对端 (较新设备) 清空并向本机请求更新。");
                        iamEffectivelyMoreUptimeDevice = true;
                        iamRequestingAllData = false;
                        SyncMessage_t promptMsg;
                        promptMsg.type = MSG_TYPE_CLEAR_AND_REQUEST_UPDATE;
                        promptMsg.senderUptime = localCurrentRawUptime;
                        promptMsg.senderOffset = localCurrentOffset;
                        memset(&promptMsg.touch_data, 0, sizeof(TouchData_t));
                        sendSyncMessage(&promptMsg);
                    }
                }
                initialSyncLogicProcessed = true;
            }
            else
            {
                // Serial.println("  收到 MSG_TYPE_UPTIME_INFO, 但已针对此对端 (raw uptime + offset) 处理过。忽略。");
            }
            break;
        }
        case MSG_TYPE_DRAW_POINT:
        {
            TouchData_t currentPointData = msg.touch_data; // Declare once at the beginning of the case
            int mapX = currentPointData.x;
            int mapY = currentPointData.y;

            if (isReceivingDrawingData)
            {
                // 场景1: 正在进行历史数据同步 (本机是请求方，已收到 SYNC_START)
                Serial.println("  处理 MSG_TYPE_DRAW_POINT 作为历史同步数据。");
                allDrawingHistory.push_back(currentPointData); // 存储历史点
                receivedHistoryPointCount++;
                updateReceiveProgress(receivedHistoryPointCount, totalPointsExpectedFromPeer);
                // 绘图逻辑
                if (currentPointData.color == TFT_BLACK) {
                    // 橡皮擦操作：绘制可变半径的圆形
                    tft.fillCircle(mapX, mapY, eraserRadius, TFT_BLACK);
                } else {
                    // 普通绘图操作
                    if (currentPointData.timestamp - lastRemoteDrawTime > TOUCH_STROKE_INTERVAL || lastRemotePoint.z == 0)
                    {
                        tft.drawPixel(mapX, mapY, currentPointData.color);
                    }
                    else
                    {
                        tft.drawLine(lastRemotePoint.x, lastRemotePoint.y, mapX, mapY, currentPointData.color);
                    }
                }
                lastRemotePoint.x = mapX;
                lastRemotePoint.y = mapY;
                lastRemotePoint.z = 1;
                lastRemoteDrawTime = currentPointData.timestamp;
                if (!isScreenOn)
                    hasNewUpdateWhileScreenOff = true;
            }
            else if (!iamRequestingAllData && !isSendingDrawingData && !isAwaitingSyncStartResponse)
            {
                // 场景2: 接收实时绘制点 (本机不处于任何请求/发送全量数据的状态)
                Serial.println("  处理 MSG_TYPE_DRAW_POINT 作为实时新笔划。");
                allDrawingHistory.push_back(currentPointData); // 实时点也需要加入历史
                // 绘图逻辑
                if (currentPointData.color == TFT_BLACK) {
                    // 橡皮擦操作：绘制可变半径的圆形
                    tft.fillCircle(mapX, mapY, eraserRadius, TFT_BLACK);
                } else {
                    // 普通绘图操作
                    if (currentPointData.timestamp - lastRemoteDrawTime > TOUCH_STROKE_INTERVAL || lastRemotePoint.z == 0)
                    {
                        tft.drawPixel(mapX, mapY, currentPointData.color);
                    }
                    else
                    {
                        tft.drawLine(lastRemotePoint.x, lastRemotePoint.y, mapX, mapY, currentPointData.color);
                    }
                }
                lastRemotePoint.x = mapX;
                lastRemotePoint.y = mapY;
                lastRemotePoint.z = 1;
                lastRemoteDrawTime = currentPointData.timestamp;
                if (!isScreenOn)
                    hasNewUpdateWhileScreenOff = true;
            }
            else
            {
                // 场景3: 正在等待同步开始、或本机正在发送数据、或本机正在请求但还未收到对方SYNC_START
                Serial.print("  收到 MSG_TYPE_DRAW_POINT 但本机处于中间同步状态 (");
                Serial.print("iamRequestingAllData: ");
                Serial.print(iamRequestingAllData);
                Serial.print(", isReceivingDrawingData: ");
                Serial.print(isReceivingDrawingData);
                Serial.print(", isSendingDrawingData: ");
                Serial.print(isSendingDrawingData);
                Serial.print(", isAwaitingSyncStartResponse: ");
                Serial.print(isAwaitingSyncStartResponse);
                Serial.println(")，忽略此点。");
            }
            break;
        }
        case MSG_TYPE_REQUEST_ALL_DRAWINGS:
        {
            Serial.println("收到 MSG_TYPE_REQUEST_ALL_DRAWINGS.");
            if (localEffectiveUptime > peerEffectiveUptime) // 本机是较旧设备，应该响应
            {
                if (iamRequestingAllData || isReceivingDrawingData || isSendingDrawingData)
                {
                    Serial.println("  但本机正在进行其他同步操作，忽略此新的 REQUEST_ALL_DRAWINGS 请求。");
                    if (peerRawUptime != 0)
                        lastKnownPeerUptime = peerRawUptime;
                    if (peerReceivedOffset != 0 || lastKnownPeerOffset != 0)
                        lastKnownPeerOffset = peerReceivedOffset;
                    initialSyncLogicProcessed = true;
                    break;
                }

                Serial.print("  决策: 本机有效运行时间较长。发送所有 ");
                Serial.print(allDrawingHistory.size());
                Serial.println(" 个点。");
                iamEffectivelyMoreUptimeDevice = true;
                // isSendingDrawingData = true; // isSendingDrawingData 将在下面设置
                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;
                initialSyncLogicProcessed = true;

                // 发送同步开始信号给请求方，表明本机即将开始发送数据
                SyncMessage_t syncStartMsgBeforeSending;
                syncStartMsgBeforeSending.type = MSG_TYPE_SYNC_START;
                syncStartMsgBeforeSending.senderUptime = localCurrentRawUptime;
                syncStartMsgBeforeSending.senderOffset = localCurrentOffset;
                memset(&syncStartMsgBeforeSending.touch_data, 0, sizeof(TouchData_t));
                syncStartMsgBeforeSending.totalPointsForSync = allDrawingHistory.size(); // 设置总点数
                sendSyncMessage(&syncStartMsgBeforeSending);
                Serial.println("  发送 MSG_TYPE_SYNC_START (准备发送历史数据，将开始分批发送)");

                // 设置状态以开始分批发送，实际发送将在 processIncomingMessages 末尾的逻辑中进行
                if (!allDrawingHistory.empty())
                {
                    updateSendProgress(0, allDrawingHistory.size()); // 初始化发送进度条
                }
                else
                {
                    hideSendProgress(); // 如果没有历史记录，则隐藏进度条
                }
                isSendingDrawingData = true;
                currentHistorySendIndex = 0;
                // 原有的 for 循环发送逻辑已移除
            }
            else
            {
                Serial.println("  决策: 收到 REQUEST_ALL_DRAWINGS，但本机有效运行时间并非较长。忽略。");
                if (peerRawUptime != 0)
                    lastKnownPeerUptime = peerRawUptime;
                if (peerReceivedOffset != 0 || lastKnownPeerOffset != 0)
                    lastKnownPeerOffset = peerReceivedOffset;
                initialSyncLogicProcessed = true;
            }
            break;
        }
        case MSG_TYPE_ALL_DRAWINGS_COMPLETE:
        {
            Serial.println("收到 MSG_TYPE_ALL_DRAWINGS_COMPLETE.");
            if (isReceivingDrawingData)
            {
                Serial.println("  同步完成 (本机为较新设备，已接收完数据)。");
                // 确保接收进度条更新到100% (如果需要，但现在隐藏由外部控制)
                // if (totalPointsExpectedFromPeer > 0) {
                //     updateReceiveProgress(receivedHistoryPointCount, totalPointsExpectedFromPeer);
                // }

                // 使用 SNTP-like 的时间同步方法：
                // Offset = (服务器发送时间戳 + 服务器已知偏移) - 客户端接收时间戳
                // localCurrentRawUptime 是在处理此消息时本机的 millis()
                relativeBootTimeOffset = (long)peerRawUptime + (long)peerReceivedOffset - localCurrentRawUptime;
                Serial.println("  使用 localCurrentRawUptime (SNTP-like) 计算 offset");

                // 记录一下 timeRequestSentForAllDrawings 的状态，但它不再用于主要计算
                if (timeRequestSentForAllDrawings == 0)
                {
                    Serial.println("  警告: timeRequestSentForAllDrawings 为 0 (本应在请求时设置).");
                }
                // else {
                //    long rtt_approx = localCurrentRawUptime - timeRequestSentForAllDrawings;
                //    Serial.print("  近似 RTT: "); Serial.println(rtt_approx);
                // }

                iamRequestingAllData = false;
                isReceivingDrawingData = false;
                isAwaitingSyncStartResponse = false;
                timeRequestSentForAllDrawings = 0;
                uptimeOfLastPeerSyncedFrom = peerRawUptime;

                hideReceiveProgress(); // 在所有状态更新后，显式隐藏接收进度条

                Serial.print("  relativeBootTimeOffset 计算并设置为: ");
                Serial.println(relativeBootTimeOffset);
                Serial.print("  uptimeOfLastPeerSyncedFrom 设置为: ");
                Serial.println(uptimeOfLastPeerSyncedFrom);
                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;

                Serial.println("  处理 UPTIME_INFO 进行同步决策。");
                Serial.print("  本地有效运行时间: ");
                Serial.print(millis() + relativeBootTimeOffset);
                Serial.print(" (原始: ");
                Serial.print(millis());
                Serial.print(", 偏移: ");
                Serial.print(relativeBootTimeOffset);
                Serial.println(")");
                Serial.print("  对端有效运行时间: ");
                Serial.print(peerEffectiveUptime);
                Serial.print(" (原始: ");
                Serial.print(peerRawUptime);
                Serial.print(", 偏移: ");
                Serial.print(peerReceivedOffset);
                Serial.println(")");
                initialSyncLogicProcessed = true;
            }
            else if (iamRequestingAllData && isAwaitingSyncStartResponse) // 异常：等待SYNC_START却收到COMPLETE
            {
                Serial.println("  异常：正在等待 SYNC_START，却收到了 ALL_DRAWINGS_COMPLETE。可能同步中断。重新发起请求。");
                // 状态: iamRequestingAllData = true, isAwaitingSyncStartResponse = true (保持)

                // 重新发送 SYNC_START (表明本机意图，因为我们仍想同步)
                SyncMessage_t syncStartMsgRetry;
                syncStartMsgRetry.type = MSG_TYPE_SYNC_START;
                syncStartMsgRetry.senderUptime = localCurrentRawUptime; // 使用当前时间
                syncStartMsgRetry.senderOffset = localCurrentOffset;
                memset(&syncStartMsgRetry.touch_data, 0, sizeof(TouchData_t));
                sendSyncMessage(&syncStartMsgRetry);
                Serial.println("  重新发送 MSG_TYPE_SYNC_START (在重新请求所有绘图前)");

                // 重新发送 REQUEST_ALL_DRAWINGS
                SyncMessage_t requestMsgRetry;
                requestMsgRetry.type = MSG_TYPE_REQUEST_ALL_DRAWINGS;
                requestMsgRetry.senderUptime = localCurrentRawUptime; // 使用当前时间
                requestMsgRetry.senderOffset = localCurrentOffset;
                memset(&requestMsgRetry.touch_data, 0, sizeof(TouchData_t));
                sendSyncMessage(&requestMsgRetry);
                timeRequestSentForAllDrawings = millis(); // 更新请求时间戳
                Serial.println("  重新发送 MSG_TYPE_REQUEST_ALL_DRAWINGS.");

                // 更新对端信息，因为这仍然是来自对端的最新（尽管可能是异常的）消息
                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;
                initialSyncLogicProcessed = true; // 标记已处理此消息
            }
            else
            {
                Serial.println("  收到 ALL_DRAWINGS_COMPLETE，但本机状态不符 (非正常完成，也非等待SYNC_START时收到)。忽略。");
                // 考虑是否也更新对端信息
                // lastKnownPeerUptime = peerRawUptime;
                // lastKnownPeerOffset = peerReceivedOffset;
            }
            break;
        }
        case MSG_TYPE_CLEAR_AND_REQUEST_UPDATE:
        {
            Serial.println("收到 MSG_TYPE_CLEAR_AND_REQUEST_UPDATE.");
            unsigned long effectiveUptimeDifference = (localEffectiveUptime > peerEffectiveUptime) ? (localEffectiveUptime - peerEffectiveUptime) : (peerEffectiveUptime - localEffectiveUptime);

            if (localEffectiveUptime < peerEffectiveUptime && effectiveUptimeDifference > EFFECTIVE_UPTIME_SYNC_THRESHOLD)
            {
                Serial.println("  决策: 本机有效运行时间较短 (超出阈值)。执行清空并请求。");
                if (iamRequestingAllData || isReceivingDrawingData || isSendingDrawingData)
                {
                    Serial.println("  但当前已有同步正在进行，忽略新的 CLEAR_AND_REQUEST_UPDATE 触发的同步请求。");
                    lastKnownPeerUptime = peerRawUptime;
                    lastKnownPeerOffset = peerReceivedOffset;
                    initialSyncLogicProcessed = true;
                    break;
                }
                iamEffectivelyMoreUptimeDevice = false;
                iamRequestingAllData = true;
                isAwaitingSyncStartResponse = true;
                allDrawingHistory.clear();

                SyncMessage_t syncStartMsgBeforeRequest3;
                syncStartMsgBeforeRequest3.type = MSG_TYPE_SYNC_START;
                syncStartMsgBeforeRequest3.senderUptime = localCurrentRawUptime;
                syncStartMsgBeforeRequest3.senderOffset = localCurrentOffset;
                memset(&syncStartMsgBeforeRequest3.touch_data, 0, sizeof(TouchData_t));
                sendSyncMessage(&syncStartMsgBeforeRequest3);
                Serial.println("  发送 MSG_TYPE_SYNC_START (在请求所有绘图前 - CLEAR_AND_REQUEST_UPDATE 路径)");

                SyncMessage_t requestMsg;
                requestMsg.type = MSG_TYPE_REQUEST_ALL_DRAWINGS;
                requestMsg.senderUptime = localCurrentRawUptime;
                requestMsg.senderOffset = localCurrentOffset;
                memset(&requestMsg.touch_data, 0, sizeof(TouchData_t));
                sendSyncMessage(&requestMsg);
                timeRequestSentForAllDrawings = millis();
                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;
                initialSyncLogicProcessed = true;
            }
            else
            {
                Serial.println("  决策: 收到 CLEAR_AND_REQUEST_UPDATE，但本机有效运行时间并非较短 (或在阈值内)。忽略。");
                if (peerRawUptime != 0)
                    lastKnownPeerUptime = peerRawUptime;
                if (peerReceivedOffset != 0 || lastKnownPeerOffset != 0)
                    lastKnownPeerOffset = peerReceivedOffset;
                initialSyncLogicProcessed = true;
            }
            break;
        }
        case MSG_TYPE_RESET_CANVAS:
        {
            Serial.println("收到 MSG_TYPE_RESET_CANVAS.");
            allDrawingHistory.clear();
            clearScreenAndCache();
            relativeBootTimeOffset = 0;
            iamEffectivelyMoreUptimeDevice = false;
            iamRequestingAllData = false;
            isAwaitingSyncStartResponse = false;
            isReceivingDrawingData = false;
            isSendingDrawingData = false;
            initialSyncLogicProcessed = false;
            lastKnownPeerUptime = 0;
            lastKnownPeerOffset = 0;
            uptimeOfLastPeerSyncedFrom = 0;
            SyncMessage_t uptimeInfoMsg;
            uptimeInfoMsg.type = MSG_TYPE_UPTIME_INFO;
            uptimeInfoMsg.senderUptime = localCurrentRawUptime;
            uptimeInfoMsg.senderOffset = relativeBootTimeOffset;
            memset(&uptimeInfoMsg.touch_data, 0, sizeof(TouchData_t));
            sendSyncMessage(&uptimeInfoMsg);
            Serial.println("  画布已重置。发送了新的 UPTIME_INFO。");
            if (!isScreenOn)
                hasNewUpdateWhileScreenOff = true;
            break;
        }
        case MSG_TYPE_SYNC_START:
        {
            Serial.println("收到 MSG_TYPE_SYNC_START");
            if (iamRequestingAllData && isAwaitingSyncStartResponse && !iamEffectivelyMoreUptimeDevice)
            {
                Serial.println("  本机作为请求方，收到响应方的 SYNC_START。准备清空并接收数据。");
                allDrawingHistory.clear();
                clearScreenAndCache();
                lastRemotePoint.x = 0;
                lastRemotePoint.y = 0;
                lastRemotePoint.z = 0;
                lastRemoteDrawTime = 0;

                isAwaitingSyncStartResponse = false;
                isReceivingDrawingData = true;

                totalPointsExpectedFromPeer = msg.totalPointsForSync;
                receivedHistoryPointCount = 0;
                if (totalPointsExpectedFromPeer > 0)
                {
                    updateReceiveProgress(receivedHistoryPointCount, totalPointsExpectedFromPeer);
                }
                else
                {
                    hideReceiveProgress(); // 如果对方没有点要发送，则隐藏进度条
                }

                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;
                Serial.print("  屏幕已清空。准备从对端 (raw uptime: ");
                Serial.print(peerRawUptime);
                Serial.print(", total points: ");
                Serial.print(totalPointsExpectedFromPeer);
                Serial.println(") 同步数据。");
            }
            else if (isSendingDrawingData)
            {
                Serial.println("  本机正在发送数据，但收到了一个 SYNC_START。可能是对方也想发起同步。忽略此 SYNC_START，继续本机发送流程。");
            }
            else if (isReceivingDrawingData)
            {
                Serial.println("  本机正在接收数据，又收到了一个 SYNC_START。可能是重复信号或来自不同设备的干扰。忽略。");
            }
            else
            {
                Serial.println("  收到 SYNC_START，但本机状态不符 (非明确的请求/响应流程中)。可能是一个错序或无关的信号。");
                lastKnownPeerUptime = peerRawUptime;
                lastKnownPeerOffset = peerReceivedOffset;
            }
            break;
        }
        case MSG_TYPE_HEARTBEAT:
        {
            // 收到心跳包，OnSyncDataRecv 中已经更新了 peerLastHeartbeat，这里可以根据需要添加调试信息
            // Serial.print("收到心跳包，来自对端 MAC (最后通信): "); // 调试信息，如果频繁可能会刷屏
            // char macStr[18];
            // snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            //          lastPeerMac[0], lastPeerMac[1], lastPeerMac[2],
            //          lastPeerMac[3], lastPeerMac[4], lastPeerMac[5]);
            // Serial.println(macStr);
            break;
        }
        default:
        {
            Serial.print("收到未知消息类型: ");
            Serial.println(msg.type);
            break;
        }
        } // End of switch (msg.type)
    } // End of while (!incomingMessageQueue.empty())

    // --- 分批发送历史数据逻辑 ---
    if (isSendingDrawingData)
    {
        const size_t BATCH_SIZE = 50; // 每批发送15个点
        size_t pointsSentThisCycle = 0;
        unsigned long batchSendStartTime = millis();        // 用于该批次消息的时间戳
        unsigned long currentSenderUptimeForMsg = millis(); // 获取一次，用于本批次所有消息
        long currentSenderOffsetForMsg = relativeBootTimeOffset;

        while (currentHistorySendIndex < allDrawingHistory.size() && pointsSentThisCycle < BATCH_SIZE)
        {
            const auto &drawData = allDrawingHistory[currentHistorySendIndex];

            SyncMessage_t historyPointMsg;
            historyPointMsg.type = MSG_TYPE_DRAW_POINT;
            historyPointMsg.senderUptime = currentSenderUptimeForMsg;
            historyPointMsg.senderOffset = currentSenderOffsetForMsg;
            historyPointMsg.touch_data = drawData;

            sendSyncMessage(&historyPointMsg);
            delay(5); // 在每个点发送后加入一个小的 delay(1)

            currentHistorySendIndex++;
            pointsSentThisCycle++;
        }

        if (currentHistorySendIndex >= allDrawingHistory.size())
        {
            // 所有数据点已发送完毕
            SyncMessage_t completeMsg;
            completeMsg.type = MSG_TYPE_ALL_DRAWINGS_COMPLETE;
            completeMsg.senderUptime = millis();
            completeMsg.senderOffset = relativeBootTimeOffset;
            memset(&completeMsg.touch_data, 0, sizeof(TouchData_t));
            sendSyncMessage(&completeMsg);

            Serial.println("  所有历史绘图数据已分批发送完毕。发送了 ALL_DRAWINGS_COMPLETE。");
            updateSendProgress(currentHistorySendIndex, allDrawingHistory.size()); // 最后更新一次确保是100%
            // hideSendProgress(); // 或者在 updateSendProgress 内部处理完成后的隐藏
            isSendingDrawingData = false;
            // currentHistorySendIndex = 0;
        }
        else if (pointsSentThisCycle > 0)
        {
            // 当前批次已发送，但还有更多数据
            updateSendProgress(currentHistorySendIndex, allDrawingHistory.size()); // 更新发送进度
            Serial.print("  分批发送：已发送 ");
            Serial.print(pointsSentThisCycle);
            Serial.print(" 个点，总计已发送 ");
            Serial.print(currentHistorySendIndex);
            Serial.print("/");
            Serial.print(allDrawingHistory.size());
            Serial.println(" 个点。");
        }
        // 如果仍在发送过程中 (isSendingDrawingData 仍为 true 且还有数据)，让出CPU
        if (isSendingDrawingData && currentHistorySendIndex < allDrawingHistory.size())
        {
            yield();
        }
    }
} // End of processIncomingMessages()

// 新增：发送心跳包
void sendHeartbeat()
{
    SyncMessage_t heartbeatMsg;
    heartbeatMsg.type = MSG_TYPE_HEARTBEAT;
    heartbeatMsg.senderUptime = millis();
    heartbeatMsg.senderOffset = relativeBootTimeOffset;
    memset(&heartbeatMsg.touch_data, 0, sizeof(TouchData_t));
    heartbeatMsg.totalPointsForSync = 0; // 心跳包不需要这个字段
    // 获取并添加内存信息
    heartbeatMsg.usedMemory = esp_get_free_heap_size(); // 使用 esp_get_free_heap_size 获取可用堆内存
    heartbeatMsg.totalMemory = ESP.getHeapSize(); // 使用 ESP.getHeapSize 获取总堆内存

    sendSyncMessage(&heartbeatMsg);
    // Serial.println("发送心跳包."); // 调试信息，如果频繁发送可能会刷屏
}

// 新增：检查对端心跳超时
void checkPeerHeartbeatTimeout()
{
    unsigned long currentTime = millis();
    // 使用一个临时的 vector 来存储需要移除的 MAC 地址，避免在迭代时修改 map
    std::vector<String> macsToRemove;

    for (auto const& [mac, lastHeartbeatTime] : peerLastHeartbeat)
    {
        if (currentTime - lastHeartbeatTime > HEARTBEAT_TIMEOUT_MS) // HEARTBEAT_TIMEOUT_MS 定义在 config.h
        {
            Serial.print("对端 ");
            Serial.print(mac);
            Serial.println(" 心跳超时，认为已下线。");
            macsToRemove.push_back(mac);
            // TODO: 在 UI 或其他地方显示对端下线的信息
            // 例如：updatePeerStatus(mac, false);
        }
    }

    // 移除超时的对端
    for (const auto& mac : macsToRemove)
    {
        peerLastHeartbeat.erase(mac);
        macSet.erase(mac); // 同时从 macSet 中移除
        peerInfoMap.erase(mac); // 新增：从对端信息 map 中移除
        // TODO: 如果需要，更新 UI 显示的对端数量
        // 例如：updatePeerCountDisplay(macSet.size());
    }
}

// 新增：获取对端信息列表
std::vector<PeerInfo_t> getPeerInfoList() {
    std::vector<PeerInfo_t> peerList;
    int count = 0;
    for (auto const& [mac, peerInfo] : peerInfoMap) {
        if (count < MAX_PEERS_TO_DISPLAY) { // 限制返回的数量
            peerList.push_back(peerInfo);
            count++;
        } else {
            break;
        }
    }
    return peerList;
}


// 重播所有绘图历史 (在屏幕上重新绘制所有点和线)
void replayAllDrawings()
{
    lastRemotePoint.x = 0;
    lastRemotePoint.y = 0;
    lastRemotePoint.z = 0;
    lastRemoteDrawTime = 0;

    for (size_t i = 0; i < allDrawingHistory.size(); ++i)
    {
        const auto &drawData = allDrawingHistory[i];
        if (drawData.isReset)
        {
            tft.fillScreen(TFT_BLACK);
            drawMainInterface();
            lastRemotePoint.x = 0;
            lastRemotePoint.y = 0;
            lastRemotePoint.z = 0;
            lastRemoteDrawTime = drawData.timestamp;
            continue;
        }
        int mapX = drawData.x;
        int mapY = drawData.y;
        if (drawData.color == TFT_BLACK) {
            // 橡皮擦操作：绘制可变半径的圆形
            tft.fillCircle(mapX, mapY, eraserRadius, TFT_BLACK);
        } else {
            // 普通绘图操作
            if (drawData.timestamp - lastRemoteDrawTime > TOUCH_STROKE_INTERVAL || lastRemotePoint.z == 0)
            {
                tft.drawPixel(mapX, mapY, drawData.color);
            }
            else
            {
                tft.drawLine(lastRemotePoint.x, lastRemotePoint.y, mapX, mapY, drawData.color);
            }
        }
        lastRemotePoint.x = mapX;
        lastRemotePoint.y = mapY;
        lastRemotePoint.z = 1;
        lastRemoteDrawTime = drawData.timestamp;
    }
}

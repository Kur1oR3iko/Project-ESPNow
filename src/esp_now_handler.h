#ifndef ESP_NOW_HANDLER_H
#define ESP_NOW_HANDLER_H

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // 用于 esp_wifi_get_mac()
#include <queue>
#include <set>
#include <map>        // 新增：用于 std::map
#include <string>     // For std::string if used by macSet, though it's std::set<String>
#include "config.h"   // 项目配置文件
#include <TFT_eSPI.h> // 需要 TFT_eSPI::color565 等，以及 tft 对象
#include "touch_handler.h" // For TS_Point type
#include "drawing_history.h" // 包含自定义绘图历史头文件和 TouchData_t 的定义

// ESP-NOW 相关数据结构定义
// TouchData_t 的定义已移至 drawing_history.h

enum MessageType_e // 使用 _e 后缀表示 enum
{
    MSG_TYPE_UPTIME_INFO,
    MSG_TYPE_DRAW_POINT,
    MSG_TYPE_REQUEST_ALL_DRAWINGS,
    MSG_TYPE_ALL_DRAWINGS_COMPLETE,
    MSG_TYPE_CLEAR_AND_REQUEST_UPDATE,
    MSG_TYPE_RESET_CANVAS,
    MSG_TYPE_SYNC_START, // 新增：同步开始信号
    MSG_TYPE_HEARTBEAT   // 新增：心跳包
};
typedef enum MessageType_e MessageType_t; // Typedef for the enum

typedef struct SyncMessage_s
{
    MessageType_t type;
    unsigned long senderUptime;
    long senderOffset;
    TouchData_t touch_data;
    uint16_t totalPointsForSync; // 新增：用于同步开始时告知总点数
    uint32_t usedMemory;         // 新增：发送方已用内存 (字节)
    uint32_t totalMemory;        // 新增：发送方总内存 (字节)
} SyncMessage_t;

// 新增：存储对端详细信息的结构体
typedef struct PeerInfo_s {
    String macAddress;
    unsigned long effectiveUptime;
    uint32_t usedMemory;
    uint32_t totalMemory;
} PeerInfo_t;


// ESP-NOW 相关全局变量 (声明为 extern)
extern esp_now_peer_info_t broadcastPeerInfo;
extern uint8_t broadcastAddress[];
extern std::queue<SyncMessage_t> incomingMessageQueue;
extern DrawingHistory allDrawingHistory;
extern std::set<String> macSet; // 用于设备计数，由 ESP-NOW 填充
extern std::map<String, unsigned long> peerLastHeartbeat; // 新增：存储每个对端的最后心跳时间
extern std::map<String, PeerInfo_t> peerInfoMap; // 新增：存储所有已知对端详细信息的 map

extern unsigned long lastKnownPeerUptime;
extern long lastKnownPeerOffset;
extern uint8_t lastPeerMac[6];
extern bool initialSyncLogicProcessed;
extern bool iamEffectivelyMoreUptimeDevice;
extern bool iamRequestingAllData;
extern bool isAwaitingSyncStartResponse;
extern bool isReceivingDrawingData;
extern bool isSendingDrawingData;
extern size_t currentHistorySendIndex;   // 新增：用于分批发送历史记录的当前索引
extern long relativeBootTimeOffset;
extern unsigned long uptimeOfLastPeerSyncedFrom;

// 触摸点处理相关 (用于远程点绘制)
extern TS_Point lastRemotePoint;      // 远程最后一点 (用于以正确的连续性重播历史记录)
extern unsigned long lastRemoteDrawTime; // 远程最后绘制时间 (用于以正确的时间/连续性重播历史记录)
// touchInterval 定义已移至 config.h 作为 TOUCH_STROKE_INTERVAL


// 函数声明
void espNowInit(); // ESP-NOW 初始化
void OnSyncDataSent(const uint8_t *mac_addr, esp_now_send_status_t status); // 发送回调
void OnSyncDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataPtr, int len); // 接收回调
void sendSyncMessage(const SyncMessage_t *msg); // 发送同步消息的辅助函数
void processIncomingMessages(); // 处理接收到的消息队列
void replayAllDrawings();       // 重播所有绘图历史 (需要 tft 对象)
void sendHeartbeat(); // 新增：发送心跳包
void checkPeerHeartbeatTimeout(); // 新增：检查对端心跳超时
std::vector<PeerInfo_t> getPeerInfoList(); // 新增：获取对端信息列表

// 注意: replayAllDrawings 函数依赖于在 esp_now_handler.cpp 中可访问的全局 tft 对象和 drawMainInterface 函数。

#endif // ESP_NOW_HANDLER_H

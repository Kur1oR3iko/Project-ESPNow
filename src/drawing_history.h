#ifndef DRAWING_HISTORY_H
#define DRAWING_HISTORY_H

#include <vector>
#include "config.h" // 假设 config.h 中没有 TouchData_t 的定义，但可能包含其他相关常量
#include <cstdint> // For uint32_t
// #include "esp_now_handler.h" // TouchData_t 的定义已移到此处

// ESP-NOW 相关数据结构定义 (从 esp_now_handler.h 移动到此处)
typedef struct TouchData_s // 使用 _s 后缀表示 struct，_t 用于 typedef 后的类型名
{
    int x;                   // 映射到屏幕的X坐标 (用于绘图)
    int y;                   // 映射到屏幕的Y坐标 (用于绘图)
    unsigned long timestamp; // 绘图动作的时间戳 (本地绘制时的 millis())
    bool isReset;            // 如果此操作是清屏重置，则为 true
    uint32_t color;          // 绘图颜色
} TouchData_t;


// 定义每个内部 vector 的最大容量
#define MAX_VECTOR_SIZE 2048

// 自定义绘图历史数据结构
class DrawingHistory {
private:
    std::vector<std::vector<TouchData_t>> history_vectors;

public:
    DrawingHistory() {
        // 初始创建一个 vector
        history_vectors.push_back(std::vector<TouchData_t>());
    }

    // 添加元素
    void push_back(const TouchData_t& data) {
        // 获取最后一个 vector 的引用
        std::vector<TouchData_t>& last_vector = history_vectors.back();

        // 如果最后一个 vector 已满，则创建一个新的 vector
        if (last_vector.size() >= MAX_VECTOR_SIZE) {
            history_vectors.push_back(std::vector<TouchData_t>());
            // 更新 last_vector 引用到新的 vector
            // 注意：这里不需要更新 last_vector 引用，因为 back() 返回的是引用，
            // push_back 会使之前的引用失效，需要重新获取
            // std::vector<TouchData_t>& new_last_vector = history_vectors.back();
            // last_vector = new_last_vector; // 这行是错误的逻辑
        }

        // 将数据添加到当前（最后一个）vector
        history_vectors.back().push_back(data); // 直接使用 back() 获取最新的 vector
    }

    // 清空所有历史记录
    void clear() {
        history_vectors.clear();
        // 清空后至少保留一个空的 vector
        history_vectors.push_back(std::vector<TouchData_t>());
    }

    // 获取总元素数量
    size_t size() const {
        size_t total_size = 0;
        for (const auto& vec : history_vectors) {
            total_size += vec.size();
        }
        return total_size;
    }

    // 检查是否为空
    bool empty() const {
        return size() == 0;
    }

    // 访问元素 (需要实现迭代器或索引访问)
    // 为了简化，先实现一个简单的索引访问，但需要考虑如何映射全局索引到内部 vector 的索引
    const TouchData_t& operator[](size_t index) const {
        // 计算元素所在的内部 vector 索引和其在内部 vector 中的索引
        size_t vector_index = index / MAX_VECTOR_SIZE;
        size_t element_index = index % MAX_VECTOR_SIZE;

        // 检查索引是否越界
        if (vector_index >= history_vectors.size() || element_index >= history_vectors[vector_index].size()) {
            // 处理越界错误，这里简单地抛出异常或返回默认值
            // 在嵌入式环境中，抛出异常可能不合适，可以考虑其他错误处理方式
            // 为了示例，这里使用 assert
            // assert(false && "Index out of bounds");
            // 或者返回一个默认构造的 TouchData_t (如果 TouchData_t 支持默认构造)
            // return TouchData_t(); // 需要 TouchData_t 支持默认构造
             // 或者更安全的做法是确保调用者不会传入越界索引
             // 或者返回一个引用到某个错误指示符
             // 这里暂时假设索引是有效的
        }

        return history_vectors[vector_index][element_index];
    }

    // TODO: 实现迭代器以支持范围for循环和其他算法

};

#endif // DRAWING_HISTORY_H

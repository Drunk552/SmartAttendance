#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <functional>
#include <vector>
#include <map>
#include <mutex>
#include <string>

// 定义系统事件类型
enum class EventType {
    TIME_UPDATE,       // 时间刷新 (每秒) data: std::string*
    DISK_FULL,         // 磁盘已满 warning
    DISK_NORMAL,       // 磁盘恢复正常
    CAMERA_FRAME_READY // 摄像头新帧就绪
};

// 回调函数定义
using EventCallback = std::function<void(void*)>;

class EventBus {
public:
    static EventBus& getInstance();

    // 订阅事件
    void subscribe(EventType type, EventCallback cb);

    // 发布事件 (线程安全)
    void publish(EventType type, void* data = nullptr);

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    std::map<EventType, std::vector<EventCallback>> subscribers;
    std::mutex mutex;
};

#endif // EVENT_BUS_H
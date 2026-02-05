#include "event_bus.h"

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

// 订阅事件
void EventBus::subscribe(EventType type, EventCallback cb) {
    std::lock_guard<std::mutex> lock(mutex);
    subscribers[type].push_back(cb);
}

// 发布事件 (线程安全)
void EventBus::publish(EventType type, void* data) {
    std::vector<EventCallback> callbacks;
    
    { // 锁的作用域仅限于复制列表
        std::lock_guard<std::mutex> lock(mutex);
        if (subscribers.find(type) != subscribers.end()) {
            callbacks = subscribers[type]; // 复制一份回调列表
        }
    } // <--- 这里自动解锁

    for (const auto& cb : callbacks) {
        cb(data); 
    }
}
#include "event_bus.h"

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

void EventBus::subscribe(EventType type, EventCallback cb) {
    std::lock_guard<std::mutex> lock(mutex);
    subscribers[type].push_back(cb);
}

void EventBus::publish(EventType type, void* data) {
    std::lock_guard<std::mutex> lock(mutex);
    if (subscribers.find(type) != subscribers.end()) {
        for (const auto& cb : subscribers[type]) {
            cb(data);
        }
    }
}
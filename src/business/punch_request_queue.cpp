/**
 * @file punch_request_queue.cpp
 * @brief 实现可停止、可排空的有界打卡请求队列。
 */

#include "punch_request_queue.h"

#include <stdexcept>
#include <utility>

namespace smart_attendance::business {

PunchRequestQueue::PunchRequestQueue(std::size_t capacity)
    : capacity_(capacity) {
    if (capacity_ == 0) {
        throw std::invalid_argument("PunchRequestQueue capacity must be positive");
    }
}

bool PunchRequestQueue::push(
    PendingPunch pending,
    const std::atomic<bool>& producerStopRequested) {
    std::unique_lock<std::mutex> lock(mutex_);
    spaceAvailable_.wait(lock, [this, &producerStopRequested] {
        return producerStopRequested.load() || queue_.size() < capacity_;
    });
    if (producerStopRequested.load()) {
        return false;
    }

    queue_.push(std::move(pending));
    dataAvailable_.notify_one();
    return true;
}

bool PunchRequestQueue::tryPush(PendingPunch pending) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= capacity_) {
        return false;
    }
    queue_.push(std::move(pending));
    dataAvailable_.notify_one();
    return true;
}

std::optional<PendingPunch> PunchRequestQueue::waitPop(
    const std::atomic<bool>& consumerStopRequested) {
    std::unique_lock<std::mutex> lock(mutex_);
    dataAvailable_.wait(lock, [this, &consumerStopRequested] {
        return consumerStopRequested.load() || !queue_.empty();
    });
    if (queue_.empty()) {
        return std::nullopt;
    }

    PendingPunch pending = std::move(queue_.front());
    queue_.pop();
    spaceAvailable_.notify_one();
    return pending;
}

void PunchRequestQueue::wakeAll() noexcept {
    dataAvailable_.notify_all();
    spaceAvailable_.notify_all();
}

void PunchRequestQueue::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<PendingPunch> empty;
    queue_.swap(empty);
    spaceAvailable_.notify_all();
}

std::size_t PunchRequestQueue::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace smart_attendance::business

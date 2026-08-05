/**
 * @file punch_request_queue.h
 * @brief 声明人脸采集 Worker 到统一打卡 Worker 的有界请求队列。
 */

#ifndef SMART_ATTENDANCE_BUSINESS_PUNCH_REQUEST_QUEUE_H
#define SMART_ATTENDANCE_BUSINESS_PUNCH_REQUEST_QUEUE_H

#include "services/punch_service.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

namespace smart_attendance::business {

struct PendingPunch {
    services::PunchRequest request;
    std::string userName;
};

/**
 * @brief 固定容量、单生产者单消费者语义的打卡请求队列。
 *
 * 本类不创建线程。队列满时 push 等待空间，停止标志置位并调用 wakeAll 后返回
 * false；消费者收到停止请求后仍会排空已有请求，再返回空值。
 */
class PunchRequestQueue final {
public:
    explicit PunchRequestQueue(std::size_t capacity);

    PunchRequestQueue(const PunchRequestQueue&) = delete;
    PunchRequestQueue& operator=(const PunchRequestQueue&) = delete;

    bool push(PendingPunch pending,
              const std::atomic<bool>& producerStopRequested);
    bool tryPush(PendingPunch pending);
    std::optional<PendingPunch> waitPop(
        const std::atomic<bool>& consumerStopRequested);
    void wakeAll() noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable dataAvailable_;
    std::condition_variable spaceAvailable_;
    std::queue<PendingPunch> queue_;
};

} // namespace smart_attendance::business

#endif // SMART_ATTENDANCE_BUSINESS_PUNCH_REQUEST_QUEUE_H

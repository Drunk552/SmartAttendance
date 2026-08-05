/**
 * @file face_punch_worker.h
 * @brief 声明人脸识别打卡请求的有界队列生产者和写库 Worker。
 */

#ifndef SMART_ATTENDANCE_BUSINESS_FACE_PUNCH_WORKER_H
#define SMART_ATTENDANCE_BUSINESS_FACE_PUNCH_WORKER_H

#include "business/punch_request_queue.h"

#include <atomic>
#include <cstddef>
#include <ctime>
#include <opencv2/core.hpp>
#include <string>

namespace smart_attendance::services {
class PunchService;
}

namespace smart_attendance::business {

class FacePunchWorker final {
public:
    explicit FacePunchWorker(std::size_t queueCapacity);

    void configure(services::PunchService& punchService) noexcept;
    bool isConfigured() const noexcept;
    /**
     * @brief 将识别结果复制为有界打卡请求；队列满时阻塞直到有空间或收到停止请求。
     * @return 成功入队返回 true；时间转换失败、队列关闭或停止时返回 false。
     */
    bool submit(int userId,
                std::time_t timestamp,
                const cv::Mat& snapshot,
                std::string userName,
                const std::atomic<bool>& stopRequested);
    /** @brief 在 TaskManager 数据库写 Worker 中串行排空请求并调用 PunchService。 */
    void run(const std::atomic<bool>& stopRequested);
    void wake() noexcept;
    void reset() noexcept;

private:
    PunchRequestQueue queue_;
    services::PunchService* punchService_{nullptr};
};

} // namespace smart_attendance::business

#endif // SMART_ATTENDANCE_BUSINESS_FACE_PUNCH_WORKER_H

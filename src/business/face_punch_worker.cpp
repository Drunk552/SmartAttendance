#include "infrastructure/logging/logger.h"
/**
 * @file face_punch_worker.cpp
 * @brief 实现识别线程与 PunchService 之间的有界异步边界。
 */

#include "business/face_punch_worker.h"

#include "services/punch_service.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <opencv2/imgcodecs.hpp>
#include <utility>
#include <vector>

namespace smart_attendance::business {

namespace {

constexpr std::size_t kMaxPunchSnapshotBytes = 512U * 1024U;

std::optional<services::PunchRequest> makePunchRequest(
    int userId,
    std::time_t timestamp,
    const cv::Mat& snapshot) {
    std::tm localTime{};
    if (::localtime_r(&timestamp, &localTime) == nullptr) {
        return std::nullopt;
    }

    std::vector<unsigned char> snapshotJpeg;
    if (!snapshot.empty()) {
        try {
            const std::vector<int> encodeParameters{
                cv::IMWRITE_JPEG_QUALITY, 85};
            if (!cv::imencode(
                    ".jpg", snapshot, snapshotJpeg, encodeParameters) ||
                snapshotJpeg.size() > kMaxPunchSnapshotBytes) {
                SA_LOG_ERROR_STREAM() << "[Warn] Punch snapshot unavailable or exceeds 512 KiB; "
                             "attendance will continue without image."
                          << std::endl;
                snapshotJpeg.clear();
            }
        } catch (const cv::Exception& error) {
            SA_LOG_ERROR_STREAM() << "[Warn] Punch snapshot encode failed: "
                      << error.what() << std::endl;
            snapshotJpeg.clear();
        }
    }

    return services::PunchRequest{
        userId,
        static_cast<std::int64_t>(timestamp),
        localTime.tm_hour * 60 + localTime.tm_min,
        std::move(snapshotJpeg)};
}

const char* punchErrorName(services::PunchError error) noexcept {
    using services::PunchError;
    switch (error) {
        case PunchError::InvalidRequest: return "InvalidRequest";
        case PunchError::ScheduleReadFailed: return "ScheduleReadFailed";
        case PunchError::NoShift: return "NoShift";
        case PunchError::RulesReadFailed: return "RulesReadFailed";
        case PunchError::AttendanceReadFailed: return "AttendanceReadFailed";
        case PunchError::DuplicatePunch: return "DuplicatePunch";
        case PunchError::InvalidRules: return "InvalidRules";
        case PunchError::InvalidShift: return "InvalidShift";
        case PunchError::WriteFailed: return "WriteFailed";
    }
    return "Unknown";
}

} // namespace

FacePunchWorker::FacePunchWorker(std::size_t queueCapacity)
    : queue_(queueCapacity) {}

void FacePunchWorker::configure(
    services::PunchService& punchService) noexcept {
    punchService_ = &punchService;
}

bool FacePunchWorker::isConfigured() const noexcept {
    return punchService_ != nullptr;
}

bool FacePunchWorker::submit(
    int userId,
    std::time_t timestamp,
    const cv::Mat& snapshot,
    std::string userName,
    const std::atomic<bool>& stopRequested) {
    auto request = makePunchRequest(userId, timestamp, snapshot);
    if (!request) {
        return false;
    }
    return queue_.push(
        {std::move(*request), std::move(userName)}, stopRequested);
}

void FacePunchWorker::run(const std::atomic<bool>& stopRequested) {
    SA_LOG_INFO_STREAM() << ">>> [Business] DB Writer thread started." << std::endl;
    while (true) {
        auto pending = queue_.waitPop(stopRequested);
        if (!pending) {
            break;
        }

        try {
            if (punchService_ == nullptr) {
                SA_LOG_ERROR_STREAM() << "[Error] PunchService is not configured." << std::endl;
                continue;
            }

            const int userId = pending->request.userId;
            const auto result = punchService_->punch(
                std::move(pending->request));
            if (result) {
                SA_LOG_INFO_STREAM() << "[Async] Save OK -> User ID: " << userId
                          << " | Status: "
                          << static_cast<int>(result.value().status)
                          << " | Diff: " << result.value().minutesDifference
                          << "m" << std::endl;
            } else {
                SA_LOG_ERROR_STREAM() << "[Async] Punch rejected -> User ID: "
                          << userId << " | Error: "
                          << punchErrorName(result.error()) << std::endl;
            }
        } catch (const std::exception& error) {
            SA_LOG_ERROR_STREAM() << "[Error] Punch Worker Exception: "
                      << error.what() << std::endl;
        } catch (...) {
            SA_LOG_ERROR_STREAM() << "[Error] Punch Worker Unknown Error!" << std::endl;
        }
    }
    SA_LOG_INFO_STREAM() << ">>> [Business] DB Writer thread stopped." << std::endl;
}

void FacePunchWorker::wake() noexcept {
    queue_.wakeAll();
}

void FacePunchWorker::reset() noexcept {
    queue_.clear();
    punchService_ = nullptr;
}

} // namespace smart_attendance::business

/**
 * @file ui_background_job_queue.cpp
 * @brief 实现有界 UI 后台任务队列。
 */

#include "ui_background_job_queue.h"

#include <utility>

namespace smart_attendance::app {

UiBackgroundJobQueue::UiBackgroundJobQueue(
    UserReportExporter userReportExporter,
    CustomReportExporter customReportExporter,
    EmployeeSettingsExporter employeeSettingsExporter,
    EmployeeSettingsImporter employeeSettingsImporter) noexcept
    : userReportExporter_(userReportExporter),
      customReportExporter_(customReportExporter),
      employeeSettingsExporter_(employeeSettingsExporter),
      employeeSettingsImporter_(employeeSettingsImporter) {}

void UiBackgroundJobQueue::configureExporters(
    UserReportExporter userReportExporter,
    CustomReportExporter customReportExporter,
    EmployeeSettingsExporter employeeSettingsExporter,
    EmployeeSettingsImporter employeeSettingsImporter) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (outstandingCount_ != 0 || !accepting_) {
        return;
    }
    userReportExporter_ = std::move(userReportExporter);
    customReportExporter_ = std::move(customReportExporter);
    employeeSettingsExporter_ = std::move(employeeSettingsExporter);
    employeeSettingsImporter_ = std::move(employeeSettingsImporter);
}

UiBackgroundJobSubmitError UiBackgroundJobQueue::submitUserReport(
    UiBackgroundJobOwner owner,
    int userId,
    std::string startDate,
    std::string endDate,
    std::uint64_t& requestId) {
    if (userId <= 0 || startDate.empty() || endDate.empty()) {
        return UiBackgroundJobSubmitError::InvalidArgument;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_) {
        return UiBackgroundJobSubmitError::Stopped;
    }
    if (outstandingCount_ >= kCapacity) {
        return UiBackgroundJobSubmitError::QueueFull;
    }

    requestId = nextRequestId_++;
    jobs_.push_back({requestId,
                     owner,
                     UiBackgroundJobType::User,
                     userId,
                     std::move(startDate),
                     std::move(endDate)});
    ++outstandingCount_;
    condition_.notify_one();
    return UiBackgroundJobSubmitError::None;
}

UiBackgroundJobSubmitError UiBackgroundJobQueue::submitCustomReport(
    UiBackgroundJobOwner owner,
    std::string startDate,
    std::string endDate,
    std::uint64_t& requestId) {
    if (startDate.empty() || endDate.empty()) {
        return UiBackgroundJobSubmitError::InvalidArgument;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_) {
        return UiBackgroundJobSubmitError::Stopped;
    }
    if (outstandingCount_ >= kCapacity) {
        return UiBackgroundJobSubmitError::QueueFull;
    }

    requestId = nextRequestId_++;
    jobs_.push_back({requestId,
                     owner,
                     UiBackgroundJobType::Custom,
                     0,
                     std::move(startDate),
                     std::move(endDate)});
    ++outstandingCount_;
    condition_.notify_one();
    return UiBackgroundJobSubmitError::None;
}

UiBackgroundJobSubmitError UiBackgroundJobQueue::submitEmployeeSettingsExport(
    UiBackgroundJobOwner owner,
    std::uint64_t& requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_) {
        return UiBackgroundJobSubmitError::Stopped;
    }
    if (outstandingCount_ >= kCapacity) {
        return UiBackgroundJobSubmitError::QueueFull;
    }

    requestId = nextRequestId_++;
    jobs_.push_back({requestId,
                     owner,
                     UiBackgroundJobType::EmployeeSettingsExport,
                     0,
                     {},
                     {}});
    ++outstandingCount_;
    condition_.notify_one();
    return UiBackgroundJobSubmitError::None;
}

UiBackgroundJobSubmitError UiBackgroundJobQueue::submitEmployeeSettingsImport(
    UiBackgroundJobOwner owner,
    std::uint64_t& requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accepting_) {
        return UiBackgroundJobSubmitError::Stopped;
    }
    if (outstandingCount_ >= kCapacity) {
        return UiBackgroundJobSubmitError::QueueFull;
    }

    requestId = nextRequestId_++;
    jobs_.push_back({requestId,
                     owner,
                     UiBackgroundJobType::EmployeeSettingsImport,
                     0,
                     {},
                     {}});
    ++outstandingCount_;
    condition_.notify_one();
    return UiBackgroundJobSubmitError::None;
}

bool UiBackgroundJobQueue::tryPopResult(UiBackgroundJobResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (results_.empty()) {
        return false;
    }

    result = results_.front();
    results_.pop_front();
    --outstandingCount_;
    return true;
}

void UiBackgroundJobQueue::run(const std::atomic<bool>& stopRequested) {
    while (true) {
        BackgroundJob job{};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [&]() {
                return !jobs_.empty() ||
                       (stopRequested.load() && !accepting_);
            });
            if (jobs_.empty() && stopRequested.load() && !accepting_) {
                break;
            }

            job = std::move(jobs_.front());
            jobs_.pop_front();
        }

        bool success = false;
        int invalidTimeCount = 0;
        try {
            if (job.type == UiBackgroundJobType::User) {
                success = userReportExporter_(
                    job.userId, job.startDate, job.endDate);
            } else if (job.type == UiBackgroundJobType::Custom) {
                success = customReportExporter_(job.startDate, job.endDate);
            } else if (job.type == UiBackgroundJobType::EmployeeSettingsExport) {
                success = employeeSettingsExporter_();
            } else {
                success = employeeSettingsImporter_(invalidTimeCount);
            }
        } catch (...) {
            success = false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        results_.push_back(
            {job.requestId, job.owner, job.type, success, invalidTimeCount});
    }
}

void UiBackgroundJobQueue::requestStop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        accepting_ = false;
    }
    condition_.notify_all();
}

bool UiBackgroundJobQueue::isValid() const noexcept {
    return userReportExporter_ != nullptr &&
           customReportExporter_ != nullptr &&
           employeeSettingsExporter_ != nullptr &&
           employeeSettingsImporter_ != nullptr;
}

} // namespace smart_attendance::app

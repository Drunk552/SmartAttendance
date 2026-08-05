/**
 * @file ui_background_job_queue.h
 * @brief 定义有界的 UI 后台请求和完成结果队列。
 */

#ifndef SMART_ATTENDANCE_APP_UI_BACKGROUND_JOB_QUEUE_H
#define SMART_ATTENDANCE_APP_UI_BACKGROUND_JOB_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <functional>
#include <string>

namespace smart_attendance::app {

using UserReportExporter = std::function<bool(int userId,
                                              const std::string& startDate,
                                              const std::string& endDate)>;
using CustomReportExporter = std::function<bool(const std::string& startDate,
                                                const std::string& endDate)>;
using EmployeeSettingsExporter = std::function<bool()>;
using EmployeeSettingsImporter = std::function<bool(int& invalidTimeCount)>;

enum class UiBackgroundJobSubmitError {
    None,
    InvalidArgument,
    QueueFull,
    Stopped
};

enum class UiBackgroundJobOwner {
    RecordQuery,
    AttendanceStatistics
};

enum class UiBackgroundJobType {
    User,
    Custom,
    EmployeeSettingsExport,
    EmployeeSettingsImport
};

struct UiBackgroundJobResult {
    std::uint64_t requestId;
    UiBackgroundJobOwner owner;
    UiBackgroundJobType type;
    bool success;
    int invalidTimeCount;
};

/**
 * @brief 保存固定上限的报表及员工设置导入导出请求和完成结果。
 *
 * submit() 和 tryPopResult() 可由 UI 线程调用；run() 只由 TaskManager Worker
 * 调用。停止后拒绝新请求，但会处理完已接受的请求。
 */
class UiBackgroundJobQueue final {
public:
    static constexpr std::size_t kCapacity = 4;

    UiBackgroundJobQueue(UserReportExporter userReportExporter,
                         CustomReportExporter customReportExporter,
                         EmployeeSettingsExporter employeeSettingsExporter,
                         EmployeeSettingsImporter employeeSettingsImporter) noexcept;

    UiBackgroundJobQueue(const UiBackgroundJobQueue&) = delete;
    UiBackgroundJobQueue& operator=(const UiBackgroundJobQueue&) = delete;

    UiBackgroundJobSubmitError submitUserReport(
        UiBackgroundJobOwner owner,
        int userId,
        std::string startDate,
        std::string endDate,
        std::uint64_t& requestId);

    UiBackgroundJobSubmitError submitCustomReport(
        UiBackgroundJobOwner owner,
        std::string startDate,
        std::string endDate,
        std::uint64_t& requestId);

    UiBackgroundJobSubmitError submitEmployeeSettingsExport(
        UiBackgroundJobOwner owner,
        std::uint64_t& requestId);

    UiBackgroundJobSubmitError submitEmployeeSettingsImport(
        UiBackgroundJobOwner owner,
        std::uint64_t& requestId);

    bool tryPopResult(UiBackgroundJobResult& result);
    void run(const std::atomic<bool>& stopRequested);
    void requestStop() noexcept;
    bool isValid() const noexcept;

private:
    struct BackgroundJob {
        std::uint64_t requestId;
        UiBackgroundJobOwner owner;
        UiBackgroundJobType type;
        int userId;
        std::string startDate;
        std::string endDate;
    };

    UserReportExporter userReportExporter_;
    CustomReportExporter customReportExporter_;
    EmployeeSettingsExporter employeeSettingsExporter_;
    EmployeeSettingsImporter employeeSettingsImporter_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<BackgroundJob> jobs_;
    std::deque<UiBackgroundJobResult> results_;
    std::size_t outstandingCount_{0};
    std::uint64_t nextRequestId_{1};
    bool accepting_{true};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_UI_BACKGROUND_JOB_QUEUE_H

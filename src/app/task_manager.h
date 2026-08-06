/**
 * @file task_manager.h
 * @brief 声明后台任务的统一启动、停止请求和等待接口。
 */

#ifndef SMART_ATTENDANCE_APP_TASK_MANAGER_H
#define SMART_ATTENDANCE_APP_TASK_MANAGER_H

#include "ui_background_job_queue.h"
#include "ui_system_status_mailbox.h"

#include <atomic>
#include <functional>
#include <thread>

namespace smart_attendance::app {

enum class TaskManagerState {
    Created,
    Running,
    StopRequested,
    Joined
};

/**
 * @brief 一个由 TaskManager 拥有的长期任务入口。
 *
 * wake 可以为空；仅在 Worker 阻塞于条件变量等可唤醒等待时提供。
 */
struct WorkerLifecycle {
    std::function<void(const std::atomic<bool>& stopRequested)> run;
    std::function<void()> wake;
};

/** @brief 系统监控 Worker 入口；状态只能写入传入的单槽 UI 邮箱。 */
struct MonitorWorkerLifecycle {
    std::function<void(const std::atomic<bool>& stopRequested,
                       UiSystemStatusMailbox& mailbox)> run;
    std::function<void()> wake;
};

/** @brief 持有单个线程及其停止标志，保证析构前完成 join。 */
class ThreadWorker final {
public:
    explicit ThreadWorker(WorkerLifecycle lifecycle) noexcept;
    ~ThreadWorker() noexcept;

    ThreadWorker(const ThreadWorker&) = delete;
    ThreadWorker& operator=(const ThreadWorker&) = delete;
    ThreadWorker(ThreadWorker&&) = delete;
    ThreadWorker& operator=(ThreadWorker&&) = delete;

    bool isValid() const noexcept;
    bool start() noexcept;
    void requestStop() noexcept;
    bool join() noexcept;

private:
    WorkerLifecycle lifecycle_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> taskFailed_{false};
    std::thread thread_;
};

/**
 * @brief 统一管理后台任务生命周期调用顺序。
 *
 * 当前直接拥有 UI 后台任务队列，以及系统监控、UI 帧投递、摄像头采集和数据库写
 * Worker。业务资源必须由 ApplicationServices 提前初始化；停止时先停止新输入并
 * 等待前四个 Worker，再排空写库 Worker。
 */
class TaskManager final {
public:
    TaskManager(UserReportExporter userReportExporter,
                CustomReportExporter customReportExporter,
                EmployeeSettingsExporter employeeSettingsExporter,
                EmployeeSettingsImporter employeeSettingsImporter,
                MonitorWorkerLifecycle monitorWorkerLifecycle,
                WorkerLifecycle frameDeliveryWorkerLifecycle,
                WorkerLifecycle captureWorkerLifecycle,
                WorkerLifecycle databaseWriterWorkerLifecycle) noexcept;
    ~TaskManager() noexcept;

    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    /** @brief 启动任务；重复启动或回调无效时返回 false。 */
    bool start() noexcept;

    /** @brief 停止 UI 后台输入，并请求监控、帧投递和采集 Worker 停止。 */
    bool requestStop() noexcept;

    /**
     * @brief 等待 UI 后台、监控、帧投递和采集 Worker 退出，再等待数据库写 Worker。
     * @return 五个 Worker 正常退出返回 true；状态错误或任务异常返回 false。
     */
    bool join() noexcept;

    TaskManagerState state() const noexcept;
    UiBackgroundJobQueue& uiBackgroundJobs() noexcept;
    UiSystemStatusMailbox& uiSystemStatus() noexcept;
    void configureReportExporters(
        UserReportExporter userReportExporter,
        CustomReportExporter customReportExporter,
        EmployeeSettingsExporter employeeSettingsExporter,
        EmployeeSettingsImporter employeeSettingsImporter) noexcept;

private:
    void stopAndJoinNoexcept() noexcept;

    UiBackgroundJobQueue uiBackgroundJobQueue_;
    UiSystemStatusMailbox uiSystemStatusMailbox_;
    ThreadWorker uiBackgroundJobWorker_;
    ThreadWorker monitorWorker_;
    ThreadWorker frameDeliveryWorker_;
    ThreadWorker captureWorker_;
    ThreadWorker databaseWriterWorker_;
    TaskManagerState state_{TaskManagerState::Created};
    bool tasksStarted_{false};
    bool joinSucceeded_{true};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_TASK_MANAGER_H

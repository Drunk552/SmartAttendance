/**
 * @file task_manager.h
 * @brief 声明后台任务的统一启动、停止请求和等待接口。
 */

#ifndef SMART_ATTENDANCE_APP_TASK_MANAGER_H
#define SMART_ATTENDANCE_APP_TASK_MANAGER_H

#include <atomic>
#include <thread>

namespace smart_attendance::app {

enum class TaskManagerState {
    Created,
    Running,
    StopRequested,
    Joined
};

/** @brief Worker 启动前执行的业务资源初始化入口。 */
struct TaskInitializer {
    bool (*initialize)();
};

/**
 * @brief 一个由 TaskManager 拥有的长期任务入口。
 *
 * wake 可以为空；仅在 Worker 阻塞于条件变量等可唤醒等待时提供。
 */
struct WorkerLifecycle {
    void (*run)(const std::atomic<bool>& stopRequested);
    void (*wake)();
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
 * 当前直接拥有摄像头采集 Worker 和数据库写 Worker。停止时先停止并等待采集
 * Worker，避免产生新任务，再唤醒并排空数据库写 Worker。
 */
class TaskManager final {
public:
    TaskManager(TaskInitializer initializer,
                WorkerLifecycle captureWorkerLifecycle,
                WorkerLifecycle databaseWriterWorkerLifecycle) noexcept;
    ~TaskManager() noexcept;

    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    /** @brief 启动任务；重复启动或回调无效时返回 false。 */
    bool start() noexcept;

    /** @brief 先请求采集 Worker 停止，阻止产生新的写库任务。 */
    bool requestStop() noexcept;

    /**
     * @brief 等待采集 Worker 退出，再唤醒并等待数据库写 Worker。
     * @return 两个 Worker 正常退出返回 true；状态错误或任务异常返回 false。
     */
    bool join() noexcept;

    TaskManagerState state() const noexcept;

private:
    void stopAndJoinNoexcept() noexcept;

    TaskInitializer initializer_;
    ThreadWorker captureWorker_;
    ThreadWorker databaseWriterWorker_;
    TaskManagerState state_{TaskManagerState::Created};
    bool tasksStarted_{false};
    bool joinSucceeded_{true};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_TASK_MANAGER_H

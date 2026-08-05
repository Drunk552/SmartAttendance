/**
 * @file task_manager.cpp
 * @brief 实现后台任务的统一生命周期编排。
 */

#include "task_manager.h"

#include <utility>

namespace smart_attendance::app {

namespace {

WorkerLifecycle bindMonitorWorker(MonitorWorkerLifecycle lifecycle,
                                  UiSystemStatusMailbox& mailbox) {
    if (!lifecycle.run) {
        return {};
    }

    return {
        [&mailbox, run = std::move(lifecycle.run)](
            const std::atomic<bool>& stopRequested) {
            run(stopRequested, mailbox);
        },
        std::move(lifecycle.wake)};
}

} // namespace

ThreadWorker::ThreadWorker(WorkerLifecycle lifecycle) noexcept
    : lifecycle_(std::move(lifecycle)) {}

ThreadWorker::~ThreadWorker() noexcept {
    requestStop();
    (void)join();
}

bool ThreadWorker::isValid() const noexcept {
    return static_cast<bool>(lifecycle_.run);
}

bool ThreadWorker::start() noexcept {
    if (!isValid() || thread_.joinable()) {
        return false;
    }

    stopRequested_.store(false);
    taskFailed_.store(false);
    try {
        thread_ = std::thread([this]() {
            try {
                lifecycle_.run(stopRequested_);
            } catch (...) {
                taskFailed_.store(true);
            }
        });
    } catch (...) {
        return false;
    }
    return true;
}

void ThreadWorker::requestStop() noexcept {
    if (!thread_.joinable()) {
        return;
    }
    stopRequested_.store(true);
    if (lifecycle_.wake) {
        try {
            lifecycle_.wake();
        } catch (...) {
            taskFailed_.store(true);
        }
    }
}

bool ThreadWorker::join() noexcept {
    if (!thread_.joinable()) {
        return true;
    }

    try {
        thread_.join();
    } catch (...) {
        return false;
    }
    return !taskFailed_.load();
}

TaskManager::TaskManager(
    UserReportExporter userReportExporter,
    CustomReportExporter customReportExporter,
    EmployeeSettingsExporter employeeSettingsExporter,
    EmployeeSettingsImporter employeeSettingsImporter,
    MonitorWorkerLifecycle monitorWorkerLifecycle,
    WorkerLifecycle frameDeliveryWorkerLifecycle,
    WorkerLifecycle captureWorkerLifecycle,
    WorkerLifecycle databaseWriterWorkerLifecycle) noexcept
    : uiBackgroundJobQueue_(userReportExporter,
                        customReportExporter,
                        employeeSettingsExporter,
                        employeeSettingsImporter),
      uiBackgroundJobWorker_({
          [this](const std::atomic<bool>& stopRequested) {
              uiBackgroundJobQueue_.run(stopRequested);
          },
          [this]() { uiBackgroundJobQueue_.requestStop(); }}),
      monitorWorker_(bindMonitorWorker(std::move(monitorWorkerLifecycle),
                                       uiSystemStatusMailbox_)),
      frameDeliveryWorker_(std::move(frameDeliveryWorkerLifecycle)),
      captureWorker_(std::move(captureWorkerLifecycle)),
      databaseWriterWorker_(std::move(databaseWriterWorkerLifecycle)) {}

TaskManager::~TaskManager() noexcept {
    stopAndJoinNoexcept();
}

bool TaskManager::start() noexcept {
    if (state_ != TaskManagerState::Created ||
        !uiBackgroundJobQueue_.isValid() ||
        !uiBackgroundJobWorker_.isValid() ||
        !monitorWorker_.isValid() ||
        !frameDeliveryWorker_.isValid() ||
        !captureWorker_.isValid() ||
        !databaseWriterWorker_.isValid()) {
        return false;
    }

    tasksStarted_ = true;
    if (!uiBackgroundJobWorker_.start()) {
        stopAndJoinNoexcept();
        return false;
    }
    if (!monitorWorker_.start()) {
        stopAndJoinNoexcept();
        return false;
    }
    if (!frameDeliveryWorker_.start()) {
        stopAndJoinNoexcept();
        return false;
    }
    if (!captureWorker_.start()) {
        stopAndJoinNoexcept();
        return false;
    }
    if (!databaseWriterWorker_.start()) {
        stopAndJoinNoexcept();
        return false;
    }

    state_ = TaskManagerState::Running;
    return true;
}

bool TaskManager::requestStop() noexcept {
    if (state_ == TaskManagerState::StopRequested ||
        state_ == TaskManagerState::Joined) {
        return true;
    }

    if (state_ == TaskManagerState::Created && !tasksStarted_) {
        state_ = TaskManagerState::StopRequested;
        return true;
    }

    uiBackgroundJobWorker_.requestStop();
    monitorWorker_.requestStop();
    frameDeliveryWorker_.requestStop();
    captureWorker_.requestStop();
    state_ = TaskManagerState::StopRequested;
    return true;
}

bool TaskManager::join() noexcept {
    if (state_ == TaskManagerState::Joined) {
        return joinSucceeded_;
    }
    if (state_ != TaskManagerState::StopRequested) {
        return false;
    }

    if (tasksStarted_) {
        const bool uiBackgroundJobSucceeded = uiBackgroundJobWorker_.join();
        const bool monitorSucceeded = monitorWorker_.join();
        const bool frameDeliverySucceeded = frameDeliveryWorker_.join();
        const bool captureSucceeded = captureWorker_.join();
        databaseWriterWorker_.requestStop();
        const bool databaseWriterSucceeded = databaseWriterWorker_.join();
        joinSucceeded_ = uiBackgroundJobSucceeded && monitorSucceeded &&
                         frameDeliverySucceeded && captureSucceeded &&
                         databaseWriterSucceeded;
        tasksStarted_ = false;
    }

    state_ = TaskManagerState::Joined;
    return joinSucceeded_;
}

TaskManagerState TaskManager::state() const noexcept {
    return state_;
}

UiBackgroundJobQueue& TaskManager::uiBackgroundJobs() noexcept {
    return uiBackgroundJobQueue_;
}

UiSystemStatusMailbox& TaskManager::uiSystemStatus() noexcept {
    return uiSystemStatusMailbox_;
}

void TaskManager::stopAndJoinNoexcept() noexcept {
    (void)requestStop();
    if (state_ == TaskManagerState::StopRequested) {
        (void)join();
    }
}

} // namespace smart_attendance::app

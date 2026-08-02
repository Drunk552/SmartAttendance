/**
 * @file task_manager.cpp
 * @brief 实现后台任务的统一生命周期编排。
 */

#include "task_manager.h"

namespace smart_attendance::app {

ThreadWorker::ThreadWorker(WorkerLifecycle lifecycle) noexcept
    : lifecycle_(lifecycle) {}

ThreadWorker::~ThreadWorker() noexcept {
    requestStop();
    (void)join();
}

bool ThreadWorker::isValid() const noexcept {
    return lifecycle_.run != nullptr;
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
    if (lifecycle_.wake != nullptr) {
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
    TaskInitializer initializer,
    WorkerLifecycle captureWorkerLifecycle,
    WorkerLifecycle databaseWriterWorkerLifecycle) noexcept
    : initializer_(initializer),
      captureWorker_(captureWorkerLifecycle),
      databaseWriterWorker_(databaseWriterWorkerLifecycle) {}

TaskManager::~TaskManager() noexcept {
    stopAndJoinNoexcept();
}

bool TaskManager::start() noexcept {
    if (state_ != TaskManagerState::Created ||
        initializer_.initialize == nullptr ||
        !captureWorker_.isValid() ||
        !databaseWriterWorker_.isValid()) {
        return false;
    }

    // 旧启动函数可能在返回失败或抛出异常前创建部分资源。
    tasksStarted_ = true;
    try {
        if (!initializer_.initialize()) {
            stopAndJoinNoexcept();
            return false;
        }
    } catch (...) {
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
        const bool captureSucceeded = captureWorker_.join();
        databaseWriterWorker_.requestStop();
        const bool databaseWriterSucceeded = databaseWriterWorker_.join();
        joinSucceeded_ = captureSucceeded && databaseWriterSucceeded;
        tasksStarted_ = false;
    }

    state_ = TaskManagerState::Joined;
    return joinSucceeded_;
}

TaskManagerState TaskManager::state() const noexcept {
    return state_;
}

void TaskManager::stopAndJoinNoexcept() noexcept {
    (void)requestStop();
    if (state_ == TaskManagerState::StopRequested) {
        (void)join();
    }
}

} // namespace smart_attendance::app

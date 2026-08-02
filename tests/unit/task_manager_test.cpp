/**
 * @file task_manager_test.cpp
 * @brief 验证 TaskManager 对采集和数据库写 Worker 的生命周期所有权。
 */

#include "app/task_manager.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

using smart_attendance::app::TaskInitializer;
using smart_attendance::app::TaskManager;
using smart_attendance::app::TaskManagerState;
using smart_attendance::app::WorkerLifecycle;

namespace {

int initializeCount = 0;
int captureRunCount = 0;
int captureExitCount = 0;
int databaseRunCount = 0;
int databaseExitCount = 0;
int databaseWakeCount = 0;
int callbackOrder = 0;
int captureExitOrder = 0;
int databaseWakeOrder = 0;
bool initializeResult = true;
bool throwOnInitialize = false;
bool throwInCaptureWorker = false;

bool fakeInitialize() {
    ++initializeCount;
    if (throwOnInitialize) {
        throw 1;
    }
    return initializeResult;
}

void fakeCaptureRun(const std::atomic<bool>& stopRequested) {
    ++captureRunCount;
    if (throwInCaptureWorker) {
        throw 1;
    }
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++captureExitCount;
    captureExitOrder = ++callbackOrder;
}

void fakeDatabaseRun(const std::atomic<bool>& stopRequested) {
    ++databaseRunCount;
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++databaseExitCount;
}

void fakeDatabaseWake() {
    ++databaseWakeCount;
    databaseWakeOrder = ++callbackOrder;
}

void resetFake(bool result = true, bool shouldThrow = false) {
    initializeCount = 0;
    captureRunCount = 0;
    captureExitCount = 0;
    databaseRunCount = 0;
    databaseExitCount = 0;
    databaseWakeCount = 0;
    callbackOrder = 0;
    captureExitOrder = 0;
    databaseWakeOrder = 0;
    initializeResult = result;
    throwOnInitialize = shouldThrow;
    throwInCaptureWorker = false;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    const TaskInitializer initializer{fakeInitialize};
    const WorkerLifecycle captureLifecycle{fakeCaptureRun, nullptr};
    const WorkerLifecycle databaseLifecycle{
        fakeDatabaseRun,
        fakeDatabaseWake
    };

    resetFake();
    {
        TaskManager manager(initializer, captureLifecycle, databaseLifecycle);
        require(manager.state() == TaskManagerState::Created,
                "task manager must start in Created state");
        require(!manager.join(),
                "join before a stop request must be rejected");
        require(manager.start(), "first start must succeed");
        require(initializeCount == 1,
                "task resources must initialize exactly once");
        require(!manager.start(), "repeated start must be rejected");

        require(manager.requestStop(), "requestStop must succeed");
        require(manager.state() == TaskManagerState::StopRequested,
                "requestStop must change the state");
        require(databaseWakeCount == 0,
                "database writer must remain active until capture joins");
        require(manager.join(), "join after requestStop must succeed");
        require(captureRunCount == 1 && captureExitCount == 1,
                "capture worker must start and exit exactly once");
        require(databaseRunCount == 1 && databaseExitCount == 1,
                "database worker must start and exit exactly once");
        require(databaseWakeCount == 1,
                "database worker must be woken exactly once");
        require(captureExitOrder < databaseWakeOrder,
                "capture must exit before database shutdown begins");
        require(manager.join(), "repeated join must be harmless");
        require(databaseWakeCount == 1,
                "repeated join must not wake the database worker twice");
    }

    resetFake();
    {
        TaskManager manager(initializer, captureLifecycle, databaseLifecycle);
        require(manager.start(), "destructor cleanup start must succeed");
    }
    require(captureExitCount == 1 && databaseExitCount == 1,
            "destructor must join both running workers");

    resetFake(false);
    {
        TaskManager manager(initializer, captureLifecycle, databaseLifecycle);
        require(!manager.start(), "reported initialization failure must propagate");
        require(manager.state() == TaskManagerState::Joined,
                "initialization failure must finish cleanup");
        require(captureRunCount == 0 && databaseRunCount == 0,
                "workers must not start after initialization failure");
    }

    resetFake(true, true);
    {
        TaskManager manager(initializer, captureLifecycle, databaseLifecycle);
        require(!manager.start(), "initialization exception must be contained");
        require(manager.state() == TaskManagerState::Joined,
                "initialization exception must finish cleanup");
    }

    resetFake();
    throwInCaptureWorker = true;
    {
        TaskManager manager(initializer, captureLifecycle, databaseLifecycle);
        require(manager.start(),
                "thread creation must succeed before capture failure");
        require(manager.requestStop(),
                "capture failure must not block stop request");
        require(!manager.join(),
                "capture exception must be reported by join");
        require(manager.state() == TaskManagerState::Joined,
                "failed capture worker must still be joined");
        require(databaseExitCount == 1,
                "database worker must still stop after capture failure");
        require(!manager.join(),
                "repeated join must preserve worker failure result");
    }

    resetFake();
    {
        TaskManager manager(
            {nullptr}, {nullptr, nullptr}, {nullptr, nullptr});
        require(!manager.start(), "invalid callbacks must be rejected");
        require(initializeCount == 0,
                "invalid callbacks must not be invoked");
    }

    std::cout << "[PASSED] task manager owns capture and database workers\n";
    return EXIT_SUCCESS;
}

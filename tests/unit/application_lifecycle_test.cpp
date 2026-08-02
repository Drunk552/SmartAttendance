/**
 * @file application_lifecycle_test.cpp
 * @brief 验证 Application 的状态、运行目录和资源释放顺序。
 */

#include "app/application.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using smart_attendance::app::Application;
using smart_attendance::app::ApplicationInitError;
using smart_attendance::app::ApplicationState;
using smart_attendance::app::DatabaseLifecycle;
using smart_attendance::app::TaskInitializer;
using smart_attendance::app::WorkerLifecycle;

namespace {

int databaseInitializeCount = 0;
int databaseCloseCount = 0;
int taskInitializeCount = 0;
int captureRunCount = 0;
int captureExitCount = 0;
int writerRunCount = 0;
int writerExitCount = 0;
int writerWakeCount = 0;
int lifecycleOrder = 0;
int captureExitOrder = 0;
int writerWakeOrder = 0;
int writerExitOrder = 0;
int databaseCloseOrder = 0;
bool databaseInitializeResult = true;
bool captureShouldThrow = false;

bool fakeDatabaseInitialize() {
    ++databaseInitializeCount;
    return databaseInitializeResult;
}

void fakeDatabaseClose() {
    ++databaseCloseCount;
    databaseCloseOrder = ++lifecycleOrder;
}

bool fakeTaskInitialize() {
    ++taskInitializeCount;
    return true;
}

void fakeCaptureRun(const std::atomic<bool>& stopRequested) {
    ++captureRunCount;
    if (captureShouldThrow) {
        throw 1;
    }
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++captureExitCount;
    captureExitOrder = ++lifecycleOrder;
}

void fakeWriterRun(const std::atomic<bool>& stopRequested) {
    ++writerRunCount;
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++writerExitCount;
    writerExitOrder = ++lifecycleOrder;
}

void fakeWriterWake() {
    ++writerWakeCount;
    writerWakeOrder = ++lifecycleOrder;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void resetFakes(bool databaseResult) {
    databaseInitializeCount = 0;
    databaseCloseCount = 0;
    taskInitializeCount = 0;
    captureRunCount = 0;
    captureExitCount = 0;
    writerRunCount = 0;
    writerExitCount = 0;
    writerWakeCount = 0;
    lifecycleOrder = 0;
    captureExitOrder = 0;
    writerWakeOrder = 0;
    writerExitOrder = 0;
    databaseCloseOrder = 0;
    databaseInitializeResult = databaseResult;
    captureShouldThrow = false;
}

std::filesystem::path uniqueRuntimeDirectory() {
    const auto suffix = std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count();
    return std::filesystem::temp_directory_path() /
           ("smartattendance_application_test_" + std::to_string(suffix));
}

} // namespace

int main() {
    const auto originalDirectory = std::filesystem::current_path();
    const auto runtimeDirectory = uniqueRuntimeDirectory();
    const DatabaseLifecycle databaseLifecycle{
        fakeDatabaseInitialize,
        fakeDatabaseClose
    };
    const TaskInitializer taskInitializer{fakeTaskInitialize};
    const WorkerLifecycle captureLifecycle{fakeCaptureRun, nullptr};
    const WorkerLifecycle writerLifecycle{fakeWriterRun, fakeWriterWake};

    resetFakes(false);
    {
        Application failedApplication(
            databaseLifecycle,
            taskInitializer,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedApplication.initialize() ==
                    ApplicationInitError::DatabaseInitializationFailed,
                "database initialization failure must be reported");
        require(failedApplication.state() == ApplicationState::Created,
                "failed initialization must keep Created state");
        require(databaseInitializeCount == 1 && databaseCloseCount == 1,
                "failed database initialization must clean partial resources");
        require(taskInitializeCount == 0 && captureRunCount == 0 &&
                    writerRunCount == 0,
                "database failure must not touch tasks");
    }
    require(databaseCloseCount == 1,
            "destructor must not close an already cleaned database twice");

    resetFakes(true);
    {
        Application application(
            databaseLifecycle,
            taskInitializer,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);

        require(application.state() == ApplicationState::Created,
                "application must start in Created state");
        require(!application.markRunning(),
                "application must not run before initialization");
        require(application.initialize() == ApplicationInitError::None,
                "application initialization must succeed");
        require(std::filesystem::current_path() == runtimeDirectory,
                "initialization must switch to runtime directory");
        require(databaseInitializeCount == 1,
                "database must initialize exactly once");
        require(application.initialize() == ApplicationInitError::InvalidState,
                "repeated initialization must be rejected");

        require(application.markRunning(),
                "Initialized -> Running transition must succeed");
        require(taskInitializeCount == 1,
                "task resources must initialize exactly once");
        require(application.requestStop(),
                "Running -> StopRequested transition must succeed");
        require(writerWakeCount == 0,
                "writer must remain active until capture joins");
        require(!application.markRunning(),
                "application must not restart after stop request");
        require(application.stop(),
                "stop must join workers and close the database");
        require(captureRunCount == 1 && captureExitCount == 1,
                "capture worker must start and exit exactly once");
        require(writerRunCount == 1 && writerExitCount == 1,
                "writer worker must start and exit exactly once");
        require(writerWakeCount == 1,
                "writer must be woken exactly once");
        require(captureExitOrder < writerWakeOrder &&
                    writerWakeOrder < writerExitOrder &&
                    writerExitOrder < databaseCloseOrder,
                "resources must stop in capture-writer-database order");
        require(application.state() == ApplicationState::Stopped,
                "application must finish in Stopped state");
        require(databaseCloseCount == 1,
                "database must close exactly once");
        require(!application.requestStop(),
                "Stopped application must reject another stop request");
    }
    require(databaseCloseCount == 1,
            "destructor must not close stopped database twice");

    resetFakes(true);
    captureShouldThrow = true;
    {
        Application failedWorkerApplication(
            databaseLifecycle,
            taskInitializer,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedWorkerApplication.initialize() ==
                    ApplicationInitError::None,
                "worker failure test must initialize database");
        require(failedWorkerApplication.markRunning(),
                "worker threads must start before failure is observed");
    }
    require(writerExitCount == 1,
            "destructor must stop writer after capture failure");
    require(databaseCloseCount == 1,
            "destructor must close database after both workers join");

    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(runtimeDirectory);

    std::cout << "[PASSED] application worker and database lifecycle\n";
    return EXIT_SUCCESS;
}

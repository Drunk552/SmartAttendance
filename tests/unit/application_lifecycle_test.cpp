/**
 * @file application_lifecycle_test.cpp
 * @brief 验证 Application 的状态、运行目录和数据库生命周期契约。
 */

#include "app/application.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using smart_attendance::app::Application;
using smart_attendance::app::ApplicationInitError;
using smart_attendance::app::ApplicationState;
using smart_attendance::app::DatabaseLifecycle;

namespace {

int initializeCount = 0;
int closeCount = 0;
bool initializeResult = true;

bool fakeDatabaseInitialize() {
    ++initializeCount;
    return initializeResult;
}

void fakeDatabaseClose() {
    ++closeCount;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void resetDatabaseFake(bool result) {
    initializeCount = 0;
    closeCount = 0;
    initializeResult = result;
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

    resetDatabaseFake(false);
    {
        Application failedApplication(databaseLifecycle, runtimeDirectory);
        require(failedApplication.initialize() ==
                    ApplicationInitError::DatabaseInitializationFailed,
                "database initialization failure must be reported");
        require(failedApplication.state() == ApplicationState::Created,
                "failed initialization must keep Created state");
        require(initializeCount == 1,
                "failed database initialization must run once");
        require(closeCount == 1,
                "failed database initialization must clean partial resources");
    }
    require(closeCount == 1,
            "destructor must not close an already cleaned database twice");

    resetDatabaseFake(true);
    {
        Application application(databaseLifecycle, runtimeDirectory);

        require(application.state() == ApplicationState::Created,
                "application must start in Created state");
        require(!application.markRunning(),
                "application must not run before initialization");
        require(application.initialize() == ApplicationInitError::None,
                "application initialization must succeed");
        require(std::filesystem::current_path() == runtimeDirectory,
                "initialization must switch to the runtime directory");
        require(initializeCount == 1,
                "database must initialize exactly once");
        require(application.initialize() == ApplicationInitError::InvalidState,
                "repeated initialization must be rejected");
        require(initializeCount == 1,
                "repeated initialization must not reopen the database");

        require(application.markRunning(),
                "Initialized -> Running transition must succeed");
        require(application.requestStop(),
                "Running -> StopRequested transition must succeed");
        require(!application.markRunning(),
                "application must not restart after stop is requested");
        require(application.stop(),
                "stop must close resources and enter Stopped state");
        require(application.state() == ApplicationState::Stopped,
                "application must finish in Stopped state");
        require(closeCount == 1,
                "database must close exactly once");
        require(!application.requestStop(),
                "Stopped application must reject another stop request");
    }
    require(closeCount == 1,
            "destructor must not close a stopped database twice");

    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(runtimeDirectory);

    std::cout << "[PASSED] application runtime and database lifecycle\n";
    return EXIT_SUCCESS;
}

/**
 * @file application.cpp
 * @brief 实现运行目录准备和数据库生命周期管理。
 */

#include "application.h"

#include <system_error>
#include <utility>

namespace smart_attendance::app {

Application::Application(DatabaseLifecycle databaseLifecycle,
                         std::filesystem::path runtimeDirectory)
    : databaseLifecycle_(databaseLifecycle),
      runtimeDirectory_(std::move(runtimeDirectory)) {}

Application::~Application() noexcept {
    closeDatabaseNoexcept();
}

ApplicationInitError Application::initialize() noexcept {
    if (state_ != ApplicationState::Created) {
        return ApplicationInitError::InvalidState;
    }

    if (databaseLifecycle_.initialize == nullptr ||
        databaseLifecycle_.close == nullptr) {
        return ApplicationInitError::InvalidDatabaseLifecycle;
    }

    std::error_code error;
    std::filesystem::create_directories(runtimeDirectory_, error);
    if (error) {
        return ApplicationInitError::RuntimeDirectoryUnavailable;
    }

    std::filesystem::current_path(runtimeDirectory_, error);
    if (error) {
        return ApplicationInitError::RuntimeDirectoryUnavailable;
    }

    // 旧 data_init() 可能在返回 false 前已经创建部分资源，因此失败路径也必须关闭。
    databaseInitialized_ = true;
    try {
        if (!databaseLifecycle_.initialize()) {
            closeDatabaseNoexcept();
            return ApplicationInitError::DatabaseInitializationFailed;
        }
    } catch (...) {
        closeDatabaseNoexcept();
        return ApplicationInitError::DatabaseInitializationFailed;
    }

    state_ = ApplicationState::Initialized;
    return ApplicationInitError::None;
}

bool Application::markRunning() noexcept {
    if (state_ != ApplicationState::Initialized) {
        return false;
    }

    state_ = ApplicationState::Running;
    return true;
}

bool Application::requestStop() noexcept {
    if (state_ != ApplicationState::Initialized &&
        state_ != ApplicationState::Running) {
        return false;
    }

    state_ = ApplicationState::StopRequested;
    return true;
}

bool Application::stop() noexcept {
    if (state_ != ApplicationState::StopRequested) {
        return false;
    }

    if (databaseInitialized_) {
        try {
            databaseLifecycle_.close();
        } catch (...) {
            return false;
        }
        databaseInitialized_ = false;
    }

    state_ = ApplicationState::Stopped;
    return true;
}

ApplicationState Application::state() const noexcept {
    return state_;
}

const std::filesystem::path& Application::runtimeDirectory() const noexcept {
    return runtimeDirectory_;
}

void Application::closeDatabaseNoexcept() noexcept {
    if (!databaseInitialized_ || databaseLifecycle_.close == nullptr) {
        return;
    }

    try {
        databaseLifecycle_.close();
    } catch (...) {
    }
    databaseInitialized_ = false;
}

} // namespace smart_attendance::app

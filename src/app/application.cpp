/**
 * @file application.cpp
 * @brief 实现运行目录、主循环和资源生命周期管理。
 */

#include "application.h"

#include <system_error>
#include <utility>

namespace smart_attendance::app {

Application::Application(DatabaseLifecycle databaseLifecycle,
                         UiLifecycle uiLifecycle,
                         ApplicationLoop applicationLoop,
                         BusinessLifecycle businessLifecycle,
                         UserReportExporter userReportExporter,
                         CustomReportExporter customReportExporter,
                         EmployeeSettingsExporter employeeSettingsExporter,
                         EmployeeSettingsImporter employeeSettingsImporter,
                         MonitorWorkerLifecycle monitorWorkerLifecycle,
                         WorkerLifecycle frameDeliveryWorkerLifecycle,
                         WorkerLifecycle captureWorkerLifecycle,
                         WorkerLifecycle databaseWriterWorkerLifecycle,
                         PlatformDevices platformDevices,
                         std::filesystem::path runtimeDirectory)
    : uiLifecycle_(uiLifecycle),
      applicationLoop_(applicationLoop),
      services_(databaseLifecycle,
                businessLifecycle,
                std::move(platformDevices)),
      employeeLookupPresenter_(services_.employeeService()),
      settingsPresenter_(services_.configService()),
      departmentPresenter_(services_.departmentService()),
      shiftPresenter_(services_.shiftService()),
      attendanceQueryPresenter_(services_.attendanceQueryService()),
      maintenancePresenter_(services_.maintenanceService()),
      taskManager_(userReportExporter,
                   customReportExporter,
                   employeeSettingsExporter,
                   employeeSettingsImporter,
                   std::move(monitorWorkerLifecycle),
                   std::move(frameDeliveryWorkerLifecycle),
                   std::move(captureWorkerLifecycle),
                   std::move(databaseWriterWorkerLifecycle)),
      runtimeDirectory_(std::move(runtimeDirectory)) {}

Application::~Application() noexcept {
    (void)taskManager_.requestStop();
    (void)taskManager_.join();
    if (taskManager_.state() == TaskManagerState::Joined) {
        (void)shutdownUiNoexcept();
        (void)services_.shutdownBusiness();
    }
}

ApplicationInitError Application::initialize() noexcept {
    if (state_ != ApplicationState::Created) {
        return ApplicationInitError::InvalidState;
    }

    if (!services_.hasValidDatabaseLifecycle()) {
        return ApplicationInitError::InvalidDatabaseLifecycle;
    }
    if (uiLifecycle_.initialize == nullptr ||
        uiLifecycle_.shutdown == nullptr) {
        return ApplicationInitError::InvalidUiLifecycle;
    }
    if (applicationLoop_.shouldStop == nullptr ||
        applicationLoop_.runOnce == nullptr) {
        return ApplicationInitError::InvalidApplicationLoop;
    }
    if (!services_.hasValidBusinessLifecycle()) {
        return ApplicationInitError::InvalidBusinessLifecycle;
    }
    if (!services_.hasCompletePlatformDevices()) {
        return ApplicationInitError::InvalidPlatformDevices;
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

    if (!services_.initializeDatabase()) {
        return ApplicationInitError::DatabaseInitializationFailed;
    }

    try {
        uiLifecycle_.initialize();
    } catch (...) {
        (void)services_.shutdownDatabase();
        return ApplicationInitError::UiInitializationFailed;
    }
    uiInitialized_ = true;

    state_ = ApplicationState::Initialized;
    return ApplicationInitError::None;
}

bool Application::markRunning() noexcept {
    if (state_ != ApplicationState::Initialized) {
        return false;
    }

    if (!services_.initializeBusiness()) {
        return false;
    }
    if (!taskManager_.start()) {
        return false;
    }

    state_ = ApplicationState::Running;
    return true;
}

ApplicationRunError Application::run() noexcept {
    if (state_ != ApplicationState::Running) {
        return ApplicationRunError::InvalidState;
    }

    try {
        while (!applicationLoop_.shouldStop()) {
            applicationLoop_.runOnce();
        }
    } catch (...) {
        return ApplicationRunError::LoopFailed;
    }
    return ApplicationRunError::None;
}

bool Application::requestStop() noexcept {
    if (state_ != ApplicationState::Initialized &&
        state_ != ApplicationState::Running) {
        return false;
    }

    if (!taskManager_.requestStop()) {
        return false;
    }
    state_ = ApplicationState::StopRequested;
    return true;
}

bool Application::stop() noexcept {
    if (state_ != ApplicationState::StopRequested) {
        return false;
    }

    const bool tasksSucceeded = taskManager_.join();
    if (taskManager_.state() != TaskManagerState::Joined) {
        return false;
    }

    const bool uiSucceeded = shutdownUiNoexcept();
    const bool businessSucceeded = services_.shutdownBusiness();
    const bool databaseSucceeded = services_.shutdownDatabase();

    if (!uiSucceeded || !businessSucceeded || !databaseSucceeded) {
        return false;
    }
    state_ = ApplicationState::Stopped;
    return tasksSucceeded;
}

ApplicationState Application::state() const noexcept {
    return state_;
}

const std::filesystem::path& Application::runtimeDirectory() const noexcept {
    return runtimeDirectory_;
}

UiBackgroundJobQueue& Application::uiBackgroundJobs() noexcept {
    return taskManager_.uiBackgroundJobs();
}

UiSystemStatusMailbox& Application::uiSystemStatus() noexcept {
    return taskManager_.uiSystemStatus();
}

services::PunchService& Application::punchService() noexcept {
    return services_.punchService();
}

biometric::face::IFaceRecognitionEngine&
Application::faceRecognitionEngine() noexcept {
    return services_.faceRecognitionEngine();
}

hal::ICamera& Application::camera() noexcept {
    return services_.camera();
}

hal::IDisplay& Application::display() noexcept {
    return services_.display();
}

hal::IKeypad& Application::keypad() noexcept {
    return services_.keypad();
}

hal::IRtc& Application::rtc() noexcept {
    return services_.rtc();
}

hal::IStorageDevice& Application::storage() noexcept {
    return services_.storage();
}

ui::EmployeeLookupPresenter& Application::employeeLookupPresenter() noexcept {
    return employeeLookupPresenter_;
}

ui::SettingsPresenter& Application::settingsPresenter() noexcept {
    return settingsPresenter_;
}

ui::DepartmentPresenter& Application::departmentPresenter() noexcept {
    return departmentPresenter_;
}

ui::ShiftPresenter& Application::shiftPresenter() noexcept {
    return shiftPresenter_;
}

ui::AttendanceQueryPresenter& Application::attendanceQueryPresenter() noexcept {
    return attendanceQueryPresenter_;
}

ui::MaintenancePresenter& Application::maintenancePresenter() noexcept {
    return maintenancePresenter_;
}


bool Application::shutdownUiNoexcept() noexcept {
    if (!uiInitialized_) {
        return true;
    }

    // 关闭回调即使失败也不重试，避免对部分销毁的 LVGL/SDL 状态重复释放。
    uiInitialized_ = false;
    try {
        uiLifecycle_.shutdown();
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace smart_attendance::app

/**
 * @file application_services.cpp
 * @brief 实现旧服务资源的显式生命周期所有权。
 */

#include "application_services.h"

#include <utility>

namespace smart_attendance::app {

ApplicationServices::ApplicationServices(
    DatabaseLifecycle databaseLifecycle,
    BusinessLifecycle businessLifecycle,
    PlatformDevices platformDevices) noexcept
    : databaseLifecycle_(databaseLifecycle),
      businessLifecycle_(businessLifecycle),
      platformDevices_(std::move(platformDevices)),
      employeeService_(employeeRepository_),
      configService_(configRepository_),
      departmentService_(departmentRepository_),
      shiftService_(shiftManagementRepository_),
      attendanceQueryService_(attendanceQueryRepository_),
      maintenanceService_(maintenanceRepository_),
      punchService_(scheduleRepository_,
                    attendanceRuleRepository_,
                    attendanceRepository_) {}

ApplicationServices::~ApplicationServices() noexcept {
    (void)shutdownBusiness();
    shutdownDatabaseNoexcept();
}

bool ApplicationServices::hasValidDatabaseLifecycle() const noexcept {
    return databaseLifecycle_.initialize != nullptr &&
           databaseLifecycle_.close != nullptr;
}

bool ApplicationServices::hasValidBusinessLifecycle() const noexcept {
    return businessLifecycle_.initialize != nullptr &&
           businessLifecycle_.shutdown != nullptr;
}

bool ApplicationServices::initializeDatabase() noexcept {
    if (!hasValidDatabaseLifecycle() || databaseActive_) {
        return false;
    }

    databaseActive_ = true;
    try {
        if (databaseLifecycle_.initialize()) {
            return true;
        }
    } catch (...) {
    }

    cleanupFailedDatabaseInitialization();
    return false;
}

bool ApplicationServices::initializeBusiness() noexcept {
    if (!hasValidBusinessLifecycle() || businessActive_) {
        return false;
    }

    // 旧初始化函数可能在报告失败前已创建模型或缓存，失败后也必须 shutdown。
    businessActive_ = true;
    try {
        return businessLifecycle_.initialize();
    } catch (...) {
        return false;
    }
}

bool ApplicationServices::shutdownBusiness() noexcept {
    if (!businessActive_) {
        return true;
    }

    // 即使回调失败也不重试，避免重复释放部分销毁的模型和缓存。
    businessActive_ = false;
    try {
        businessLifecycle_.shutdown();
    } catch (...) {
        return false;
    }
    return true;
}

bool ApplicationServices::shutdownDatabase() noexcept {
    if (!databaseActive_) {
        return true;
    }

    try {
        databaseLifecycle_.close();
    } catch (...) {
        return false;
    }
    databaseActive_ = false;
    return true;
}

services::PunchService& ApplicationServices::punchService() noexcept {
    return punchService_;
}

services::EmployeeService& ApplicationServices::employeeService() noexcept {
    return employeeService_;
}

storage::IConfigRepository& ApplicationServices::configRepository() noexcept {
    return configRepository_;
}

services::ConfigService& ApplicationServices::configService() noexcept {
    return configService_;
}

services::DepartmentService& ApplicationServices::departmentService() noexcept {
    return departmentService_;
}

services::ShiftService& ApplicationServices::shiftService() noexcept {
    return shiftService_;
}

services::AttendanceQueryService& ApplicationServices::attendanceQueryService() noexcept {
    return attendanceQueryService_;
}

services::MaintenanceService& ApplicationServices::maintenanceService() noexcept {
    return maintenanceService_;
}

biometric::face::IFaceRecognitionEngine&
ApplicationServices::faceRecognitionEngine() noexcept {
    return faceRecognitionEngine_;
}

hal::ICamera& ApplicationServices::camera() noexcept {
    return *platformDevices_.camera;
}

hal::IDisplay& ApplicationServices::display() noexcept {
    return *platformDevices_.display;
}

hal::IKeypad& ApplicationServices::keypad() noexcept {
    return *platformDevices_.keypad;
}

hal::IRtc& ApplicationServices::rtc() noexcept {
    return *platformDevices_.rtc;
}

hal::IStorageDevice& ApplicationServices::storage() noexcept {
    return *platformDevices_.storage;
}

const hal::DeviceCapabilities&
ApplicationServices::deviceCapabilities() const noexcept {
    return platformDevices_.capabilities;
}

bool ApplicationServices::hasCompletePlatformDevices() const noexcept {
    return platformDevices_.isComplete();
}

void ApplicationServices::cleanupFailedDatabaseInitialization() noexcept {
    if (!databaseActive_) {
        return;
    }

    try {
        databaseLifecycle_.close();
    } catch (...) {
    }
    databaseActive_ = false;
}

void ApplicationServices::shutdownDatabaseNoexcept() noexcept {
    if (!databaseActive_) {
        return;
    }

    try {
        databaseLifecycle_.close();
    } catch (...) {
    }
    databaseActive_ = false;
}

} // namespace smart_attendance::app

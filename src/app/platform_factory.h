/**
 * @file platform_factory.h
 * @brief 声明当前构建平台设备集合及统一创建入口。
 */

#ifndef SMART_ATTENDANCE_APP_PLATFORM_FACTORY_H
#define SMART_ATTENDANCE_APP_PLATFORM_FACTORY_H

#include "core/common/result.h"
#include "hal/camera.h"
#include "hal/device_error.h"
#include "hal/display.h"
#include "hal/keypad.h"
#include "hal/rtc.h"
#include "hal/storage_device.h"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace smart_attendance::app {

struct PlatformConfig {
    int displayWidth{240};
    int displayHeight{320};
    std::uint16_t simulatorCameraPort{5004};
    int cameraProbeTimeoutMilliseconds{200};
    std::filesystem::path simulatedStorageRoot;
};

enum class PlatformInitError {
    InvalidDisplaySize,
    InvalidCameraConfiguration,
    InvalidStorageRoot,
    AllocationFailed
};

struct PlatformDevices {
    std::unique_ptr<hal::ICamera> camera;
    std::unique_ptr<hal::IDisplay> display;
    std::unique_ptr<hal::IKeypad> keypad;
    std::unique_ptr<hal::IRtc> rtc;
    std::unique_ptr<hal::IStorageDevice> storage;
    hal::DeviceCapabilities capabilities;

    bool isComplete() const noexcept {
        return camera != nullptr && display != nullptr && keypad != nullptr &&
               rtc != nullptr && storage != nullptr;
    }
};

/**
 * @brief 创建当前 CMake 所选平台的完整设备对象，不启动线程或打开设备。
 * @return 配置有效时返回全部非空设备；失败返回具体配置或分配错误。
 */
Result<PlatformDevices, PlatformInitError> createPlatformDevices(
    const PlatformConfig& config);

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_PLATFORM_FACTORY_H

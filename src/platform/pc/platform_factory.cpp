/**
 * @file platform_factory.cpp
 * @brief 创建 PC/WSL 仿真平台的完整 HAL 对象集合。
 */

#include "app/platform_factory.h"

#include "platform/pc/gstreamer_camera.h"
#include "platform/pc/path_storage.h"
#include "platform/pc/sdl_display.h"
#include "platform/pc/sdl_keypad.h"
#include "platform/pc/system_rtc.h"

#include <memory>

namespace smart_attendance::app {

Result<PlatformDevices, PlatformInitError> createPlatformDevices(
    const PlatformConfig& config) {
    using ResultType = Result<PlatformDevices, PlatformInitError>;
    if (config.displayWidth <= 0 || config.displayHeight <= 0) {
        return ResultType::failure(PlatformInitError::InvalidDisplaySize);
    }
    if (config.simulatorCameraPort == 0 ||
        config.cameraProbeTimeoutMilliseconds <= 0) {
        return ResultType::failure(
            PlatformInitError::InvalidCameraConfiguration);
    }
    if (config.simulatedStorageRoot.empty() ||
        !config.simulatedStorageRoot.is_absolute()) {
        return ResultType::failure(PlatformInitError::InvalidStorageRoot);
    }

    try {
        PlatformDevices devices;
        devices.camera = std::make_unique<platform::pc::GstreamerCamera>(
            config.simulatorCameraPort,
            config.cameraProbeTimeoutMilliseconds);
        devices.display = std::make_unique<platform::pc::SdlDisplay>(
            config.displayWidth, config.displayHeight);
        devices.keypad = std::make_unique<platform::pc::SdlKeypad>();
        devices.rtc = std::make_unique<platform::pc::SystemRtc>();
        devices.storage = std::make_unique<platform::pc::PathStorage>(
            config.simulatedStorageRoot);
        devices.capabilities = hal::DeviceCapabilities{
            true,
            true,
            true,
            true,
            false,
            true};
        return ResultType::success(std::move(devices));
    } catch (...) {
        return ResultType::failure(PlatformInitError::AllocationFailed);
    }
}

} // namespace smart_attendance::app

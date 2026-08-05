/**
 * @file system_rtc.cpp
 * @brief 实现 PC 仿真系统时钟读取；不修改宿主机时间。
 */

#include "platform/pc/system_rtc.h"

#include <chrono>

namespace smart_attendance::platform::pc {

Result<hal::SystemTime, hal::DeviceError> SystemRtc::now() const {
    using ResultType = Result<hal::SystemTime, hal::DeviceError>;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (seconds < 0) {
        return ResultType::failure(hal::DeviceError::ReadFailed);
    }
    return ResultType::success(hal::SystemTime{seconds});
}

Result<void, hal::DeviceError> SystemRtc::set(hal::SystemTime) {
    return Result<void, hal::DeviceError>::failure(
        hal::DeviceError::Unsupported);
}

bool SystemRtc::isWritable() const noexcept {
    return false;
}

} // namespace smart_attendance::platform::pc

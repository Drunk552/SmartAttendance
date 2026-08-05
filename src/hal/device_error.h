/**
 * @file device_error.h
 * @brief 定义平台无关的设备错误和能力描述。
 */

#ifndef SMART_ATTENDANCE_HAL_DEVICE_ERROR_H
#define SMART_ATTENDANCE_HAL_DEVICE_ERROR_H

namespace smart_attendance::hal {

enum class DeviceError {
    InvalidConfiguration,
    Unavailable,
    NotInitialized,
    AlreadyInitialized,
    OpenFailed,
    ReadFailed,
    NoFrame,
    FileSystemError,
    InvalidPath,
    Unsupported
};

struct DeviceCapabilities {
    bool camera{false};
    bool display{false};
    bool keypad{false};
    bool systemClock{false};
    bool writableRtc{false};
    bool removableStorage{false};
};

} // namespace smart_attendance::hal

#endif // SMART_ATTENDANCE_HAL_DEVICE_ERROR_H

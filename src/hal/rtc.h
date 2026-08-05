/**
 * @file rtc.h
 * @brief 定义业务系统时间和可选硬件 RTC 能力。
 */

#ifndef SMART_ATTENDANCE_HAL_RTC_H
#define SMART_ATTENDANCE_HAL_RTC_H

#include "core/common/result.h"
#include "hal/device_error.h"

#include <cstdint>

namespace smart_attendance::hal {

struct SystemTime {
    std::int64_t unixSeconds{0};
};

class IRtc {
public:
    virtual ~IRtc() = default;

    /** @brief 同步读取当前业务时间；失败返回明确设备错误。 */
    virtual Result<SystemTime, DeviceError> now() const = 0;
    /** @brief 设置时钟；不支持写入的平台返回 Unsupported。 */
    virtual Result<void, DeviceError> set(SystemTime time) = 0;
    virtual bool isWritable() const noexcept = 0;
};

} // namespace smart_attendance::hal

#endif // SMART_ATTENDANCE_HAL_RTC_H

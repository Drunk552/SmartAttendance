/**
 * @file system_rtc.h
 * @brief 声明使用主机系统时间模拟的只读 RTC。
 */

#ifndef SMART_ATTENDANCE_PLATFORM_PC_SYSTEM_RTC_H
#define SMART_ATTENDANCE_PLATFORM_PC_SYSTEM_RTC_H

#include "hal/rtc.h"

namespace smart_attendance::platform::pc {

class SystemRtc final : public hal::IRtc {
public:
    Result<hal::SystemTime, hal::DeviceError> now() const override;
    Result<void, hal::DeviceError> set(hal::SystemTime time) override;
    bool isWritable() const noexcept override;
};

} // namespace smart_attendance::platform::pc

#endif // SMART_ATTENDANCE_PLATFORM_PC_SYSTEM_RTC_H

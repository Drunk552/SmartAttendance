/**
 * @file display.h
 * @brief 定义 UI 显示后端的最小生命周期接口。
 */

#ifndef SMART_ATTENDANCE_HAL_DISPLAY_H
#define SMART_ATTENDANCE_HAL_DISPLAY_H

#include "core/common/result.h"
#include "hal/device_error.h"

namespace smart_attendance::hal {

class IDisplay {
public:
    virtual ~IDisplay() = default;

    /** @brief 由 UI 主线程在 LVGL 初始化后调用；不会创建后台线程。 */
    virtual Result<void, DeviceError> initialize() = 0;
    /** @brief 由 UI 主线程在 LVGL 反初始化前调用；重复调用安全。 */
    virtual void shutdown() noexcept = 0;
    virtual int width() const noexcept = 0;
    virtual int height() const noexcept = 0;
};

} // namespace smart_attendance::hal

#endif // SMART_ATTENDANCE_HAL_DISPLAY_H

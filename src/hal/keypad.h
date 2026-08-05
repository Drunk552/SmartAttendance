/**
 * @file keypad.h
 * @brief 定义 LVGL 输入后端的最小生命周期接口。
 */

#ifndef SMART_ATTENDANCE_HAL_KEYPAD_H
#define SMART_ATTENDANCE_HAL_KEYPAD_H

#include "core/common/result.h"
#include "hal/device_error.h"

namespace smart_attendance::hal {

class IKeypad {
public:
    virtual ~IKeypad() = default;

    /** @brief 由 UI 主线程创建输入驱动；UI 随后负责绑定全部 keypad 到焦点组。 */
    virtual Result<void, DeviceError> initialize() = 0;
    /** @brief 由 UI 主线程在 LVGL 反初始化前调用；重复调用安全。 */
    virtual void shutdown() noexcept = 0;
};

} // namespace smart_attendance::hal

#endif // SMART_ATTENDANCE_HAL_KEYPAD_H

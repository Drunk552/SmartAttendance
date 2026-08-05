/**
 * @file sdl_keypad.h
 * @brief 声明 LVGL SDL 鼠标和键盘输入后端。
 */

#ifndef SMART_ATTENDANCE_PLATFORM_PC_SDL_KEYPAD_H
#define SMART_ATTENDANCE_PLATFORM_PC_SDL_KEYPAD_H

#include "hal/keypad.h"

#include <lvgl.h>

namespace smart_attendance::platform::pc {

class SdlKeypad final : public hal::IKeypad {
public:
    Result<void, hal::DeviceError> initialize() override;
    void shutdown() noexcept override;

private:
    lv_indev_t* mouse_{nullptr};
    lv_indev_t* keypad_{nullptr};
};

} // namespace smart_attendance::platform::pc

#endif // SMART_ATTENDANCE_PLATFORM_PC_SDL_KEYPAD_H

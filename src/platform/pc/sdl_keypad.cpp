/**
 * @file sdl_keypad.cpp
 * @brief 实现 PC/WSL 的 LVGL SDL 输入设备创建与释放。
 */

#include "platform/pc/sdl_keypad.h"

namespace smart_attendance::platform::pc {

Result<void, hal::DeviceError> SdlKeypad::initialize() {
    using ResultType = Result<void, hal::DeviceError>;
    if (mouse_ != nullptr || keypad_ != nullptr) {
        return ResultType::failure(hal::DeviceError::AlreadyInitialized);
    }

    mouse_ = lv_sdl_mouse_create();
    keypad_ = lv_sdl_keyboard_create();
    if (mouse_ == nullptr || keypad_ == nullptr) {
        shutdown();
        return ResultType::failure(hal::DeviceError::Unavailable);
    }
    lv_indev_set_type(keypad_, LV_INDEV_TYPE_KEYPAD);
    return ResultType::success();
}

void SdlKeypad::shutdown() noexcept {
    if (keypad_ != nullptr) {
        lv_indev_delete(keypad_);
        keypad_ = nullptr;
    }
    if (mouse_ != nullptr) {
        lv_indev_delete(mouse_);
        mouse_ = nullptr;
    }
}

} // namespace smart_attendance::platform::pc

/**
 * @file sdl_display.cpp
 * @brief 实现 PC/WSL 的 LVGL SDL 窗口。
 */

#include "platform/pc/sdl_display.h"

namespace smart_attendance::platform::pc {

SdlDisplay::SdlDisplay(int width, int height) noexcept
    : width_(width), height_(height) {}

Result<void, hal::DeviceError> SdlDisplay::initialize() {
    using ResultType = Result<void, hal::DeviceError>;
    if (display_ != nullptr) {
        return ResultType::failure(hal::DeviceError::AlreadyInitialized);
    }
    if (width_ <= 0 || height_ <= 0) {
        return ResultType::failure(hal::DeviceError::InvalidConfiguration);
    }

    display_ = lv_sdl_window_create(width_, height_);
    if (display_ == nullptr) {
        return ResultType::failure(hal::DeviceError::Unavailable);
    }
    return ResultType::success();
}

void SdlDisplay::shutdown() noexcept {
    if (display_ != nullptr) {
        lv_display_delete(display_);
        display_ = nullptr;
    }
}

int SdlDisplay::width() const noexcept {
    return width_;
}

int SdlDisplay::height() const noexcept {
    return height_;
}

} // namespace smart_attendance::platform::pc

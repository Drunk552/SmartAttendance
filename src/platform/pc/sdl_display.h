/**
 * @file sdl_display.h
 * @brief 声明 LVGL SDL 显示后端。
 */

#ifndef SMART_ATTENDANCE_PLATFORM_PC_SDL_DISPLAY_H
#define SMART_ATTENDANCE_PLATFORM_PC_SDL_DISPLAY_H

#include "hal/display.h"

#include <lvgl.h>

namespace smart_attendance::platform::pc {

class SdlDisplay final : public hal::IDisplay {
public:
    SdlDisplay(int width, int height) noexcept;

    Result<void, hal::DeviceError> initialize() override;
    void shutdown() noexcept override;
    int width() const noexcept override;
    int height() const noexcept override;

private:
    int width_;
    int height_;
    lv_display_t* display_{nullptr};
};

} // namespace smart_attendance::platform::pc

#endif // SMART_ATTENDANCE_PLATFORM_PC_SDL_DISPLAY_H

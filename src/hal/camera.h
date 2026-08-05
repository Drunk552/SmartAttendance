/**
 * @file camera.h
 * @brief 定义摄像头生命周期和具有明确所有权的 BGR 图像帧。
 */

#ifndef SMART_ATTENDANCE_HAL_CAMERA_H
#define SMART_ATTENDANCE_HAL_CAMERA_H

#include "core/common/result.h"
#include "hal/device_error.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace smart_attendance::hal {

enum class PixelFormat {
    Bgr888
};

/** @brief 只读图像帧；data 的共享所有权覆盖所有像素访问。 */
struct CameraFrame {
    std::shared_ptr<const std::uint8_t> data;
    std::size_t dataSize{0};
    int width{0};
    int height{0};
    std::size_t strideBytes{0};
    PixelFormat format{PixelFormat::Bgr888};

    bool isValid() const noexcept {
        if (data == nullptr || width <= 0 || height <= 0 ||
            static_cast<std::size_t>(width) >
                std::numeric_limits<std::size_t>::max() / 3U) {
            return false;
        }
        const auto heightValue = static_cast<std::size_t>(height);
        return strideBytes >= static_cast<std::size_t>(width) * 3U &&
               strideBytes <= dataSize / heightValue;
    }
};

/**
 * @brief 平台无关摄像头接口，不创建线程。
 *
 * open/read 可阻塞，但平台实现必须限制单次阻塞时间。调用者拥有重试策略，且必须
 * 在 Worker 退出后调用 close。返回帧为只读共享所有权，不依赖调用栈对象。
 */
class ICamera {
public:
    virtual ~ICamera() = default;

    virtual Result<void, DeviceError> open() = 0;
    virtual Result<CameraFrame, DeviceError> read() = 0;
    virtual bool isOpen() const noexcept = 0;
    virtual void close() noexcept = 0;
};

} // namespace smart_attendance::hal

#endif // SMART_ATTENDANCE_HAL_CAMERA_H

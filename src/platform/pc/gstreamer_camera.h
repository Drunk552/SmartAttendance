/**
 * @file gstreamer_camera.h
 * @brief 声明 PC/WSL UDP GStreamer 摄像头实现。
 */

#ifndef SMART_ATTENDANCE_PLATFORM_PC_GSTREAMER_CAMERA_H
#define SMART_ATTENDANCE_PLATFORM_PC_GSTREAMER_CAMERA_H

#include "hal/camera.h"

#include <cstdint>
#include <opencv2/videoio.hpp>
#include <string>

namespace smart_attendance::platform::pc {

class GstreamerCamera final : public hal::ICamera {
public:
    GstreamerCamera(std::uint16_t udpPort, int probeTimeoutMilliseconds) noexcept;

    Result<void, hal::DeviceError> open() override;
    Result<hal::CameraFrame, hal::DeviceError> read() override;
    bool isOpen() const noexcept override;
    void close() noexcept override;

private:
    bool probeStream() const noexcept;
    std::string pipeline() const;

    std::uint16_t udpPort_;
    int probeTimeoutMilliseconds_;
    cv::VideoCapture capture_;
};

} // namespace smart_attendance::platform::pc

#endif // SMART_ATTENDANCE_PLATFORM_PC_GSTREAMER_CAMERA_H

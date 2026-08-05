/**
 * @file gstreamer_camera.cpp
 * @brief 实现当前 Windows 推流到 WSL 的兼容摄像头管线。
 */

#include "platform/pc/gstreamer_camera.h"

#include <cerrno>
#include <memory>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace smart_attendance::platform::pc {

namespace {

class ScopedFileDescriptor final {
public:
    explicit ScopedFileDescriptor(int descriptor) noexcept
        : descriptor_(descriptor) {}

    ~ScopedFileDescriptor() noexcept {
        if (descriptor_ >= 0) {
            close(descriptor_);
        }
    }

    ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
    ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;

    int get() const noexcept {
        return descriptor_;
    }

private:
    int descriptor_;
};

} // namespace

GstreamerCamera::GstreamerCamera(
    std::uint16_t udpPort,
    int probeTimeoutMilliseconds) noexcept
    : udpPort_(udpPort),
      probeTimeoutMilliseconds_(probeTimeoutMilliseconds) {}

Result<void, hal::DeviceError> GstreamerCamera::open() {
    using ResultType = Result<void, hal::DeviceError>;
    if (capture_.isOpened()) {
        return ResultType::failure(hal::DeviceError::AlreadyInitialized);
    }
    if (udpPort_ == 0 || probeTimeoutMilliseconds_ <= 0) {
        return ResultType::failure(hal::DeviceError::InvalidConfiguration);
    }
    if (!probeStream()) {
        return ResultType::failure(hal::DeviceError::Unavailable);
    }

    try {
        capture_.open(pipeline(), cv::CAP_GSTREAMER);
        if (!capture_.isOpened()) {
            capture_.release();
            return ResultType::failure(hal::DeviceError::OpenFailed);
        }
    } catch (const cv::Exception&) {
        capture_.release();
        return ResultType::failure(hal::DeviceError::OpenFailed);
    }
    return ResultType::success();
}

Result<hal::CameraFrame, hal::DeviceError> GstreamerCamera::read() {
    using ResultType = Result<hal::CameraFrame, hal::DeviceError>;
    if (!capture_.isOpened()) {
        return ResultType::failure(hal::DeviceError::NotInitialized);
    }

    try {
        auto image = std::make_shared<cv::Mat>();
        if (!capture_.read(*image) || image->empty()) {
            return ResultType::failure(hal::DeviceError::NoFrame);
        }
        if (image->type() != CV_8UC3 || !image->data) {
            return ResultType::failure(hal::DeviceError::ReadFailed);
        }

        std::shared_ptr<const std::uint8_t> pixels(image, image->data);
        return ResultType::success(hal::CameraFrame{
            std::move(pixels),
            image->step[0] * static_cast<std::size_t>(image->rows),
            image->cols,
            image->rows,
            image->step[0],
            hal::PixelFormat::Bgr888});
    } catch (const cv::Exception&) {
        return ResultType::failure(hal::DeviceError::ReadFailed);
    } catch (...) {
        return ResultType::failure(hal::DeviceError::ReadFailed);
    }
}

bool GstreamerCamera::isOpen() const noexcept {
    return capture_.isOpened();
}

void GstreamerCamera::close() noexcept {
    capture_.release();
}

bool GstreamerCamera::probeStream() const noexcept {
    const ScopedFileDescriptor socketFd(
        socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
    if (socketFd.get() < 0) {
        return false;
    }

    const int reuseAddress = 1;
    (void)setsockopt(socketFd.get(), SOL_SOCKET, SO_REUSEADDR,
                     &reuseAddress, sizeof(reuseAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(udpPort_);
    if (bind(socketFd.get(), reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0) {
        return false;
    }

    pollfd descriptor{};
    descriptor.fd = socketFd.get();
    descriptor.events = POLLIN;
    const int pollResult = poll(
        &descriptor, 1, probeTimeoutMilliseconds_);
    if (pollResult > 0 && (descriptor.revents & POLLIN) != 0) {
        return true;
    }
    if (pollResult < 0 && errno == EINTR) {
        return false;
    }
    return false;
}

std::string GstreamerCamera::pipeline() const {
    std::ostringstream stream;
    stream
        << "udpsrc port=" << udpPort_ << " timeout=2000000000 ! "
        << "application/x-rtp, media=(string)video, clock-rate=(int)90000, "
        << "encoding-name=(string)RAW, sampling=(string)YCbCr-4:2:2, "
        << "depth=(string)8, width=(string)640, height=(string)480, "
        << "colorimetry=(string)BT601-5, payload=(int)96 ! "
        << "rtpjitterbuffer latency=0 ! "
        << "rtpvrawdepay ! videoconvert ! "
        << "video/x-raw,format=BGR ! "
        << "appsink sync=false drop=true max-buffers=1";
    return stream.str();
}

} // namespace smart_attendance::platform::pc

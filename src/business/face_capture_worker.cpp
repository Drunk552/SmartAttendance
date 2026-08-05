/**
 * @file face_capture_worker.cpp
 * @brief 实现 PC 仿真摄像头采集循环并调用独立人脸算法模块。
 */

#include "business/face_capture_worker.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <opencv2/imgproc.hpp>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace smart_attendance::business {

namespace {

constexpr std::uint16_t kSimulatorStreamPort = 5004;
constexpr int kStreamProbeTimeoutMs = 200;
constexpr int kSkippedFrames = 4;
constexpr int kRecognitionCooldownMs = 2000;

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

FaceCaptureWorker::FaceCaptureWorker(
    biometric::face::IFaceRecognitionEngine& recognitionEngine,
    FaceCaptureWorkerCallbacks callbacks)
    : recognitionEngine_(recognitionEngine),
      callbacks_(std::move(callbacks)) {}

void FaceCaptureWorker::run(
    const std::atomic<bool>& stopRequested) {
    std::cout << ">>> [Business] Background capture thread started." << std::endl;

    int frameCounter = 0;
    cv::Rect lastFaceRegion;
    bool tracking = false;
    std::map<int, std::chrono::steady_clock::time_point> userCooldowns;
    auto lastUiUpdateTime = std::chrono::steady_clock::now();
    int consecutiveFailures = 0;
    int retryCount = 0;

    while (!stopRequested.load()) {
        try {
            if (!capture_.isOpened()) {
                if (++retryCount % 10 == 0) {
                    std::cout << "[Stream] 尝试重连 SDP..." << std::endl;
                    capture_.release();
                    if (waitForSimulatorStream(stopRequested)) {
                        capture_ = openSimulatorStream();
                    }
                    if (capture_.isOpened()) {
                        consecutiveFailures = 0;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            cv::Mat frame;
            if (!capture_.read(frame) || frame.empty()) {
                ++consecutiveFailures;
                if (consecutiveFailures > 60) {
                    std::cerr << "[Stream] 严重错误：流已中断，强制重启连接！"
                              << std::endl;
                    capture_.release();
                    consecutiveFailures = 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            cv::Mat processFrame = frame.clone();
            callbacks_.publishCurrentFrame(processFrame);

            const bool performDetection =
                frameCounter % (kSkippedFrames + 1) == 0;
            ++frameCounter;

            bool hasFace = false;
            cv::Rect faceRegion;
            if (performDetection) {
                auto detection = recognitionEngine_.detectLargest(processFrame);
                if (!detection) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(500));
                    continue;
                }
                if (detection.value()) {
                    faceRegion = *detection.value();
                    lastFaceRegion = faceRegion;
                    tracking = true;
                    hasFace = true;
                } else {
                    tracking = false;
                }
            } else if (tracking) {
                faceRegion = lastFaceRegion;
                hasFace = true;
            }

            if (hasFace) {
                const cv::Scalar color = performDetection
                    ? cv::Scalar(0, 255, 0)
                    : cv::Scalar(0, 255, 200);
                cv::rectangle(processFrame, faceRegion, color, 2);

                if (performDetection && callbacks_.recognitionEnabled() &&
                    recognitionEngine_.isTrained()) {
                    auto recognition = recognitionEngine_.recognize(
                        processFrame,
                        faceRegion,
                        callbacks_.preprocessConfig());
                    if (recognition && recognition.value().userId != -1 &&
                        recognition.value().confidence < 100.0) {
                        const int userId = recognition.value().userId;
                        const std::string userName =
                            callbacks_.resolveUserName(userId);
                        std::string text = userName;
                        const auto now = std::chrono::steady_clock::now();
                        bool inCooldown = false;
                        const auto cooldown = userCooldowns.find(userId);
                        if (cooldown != userCooldowns.end()) {
                            const auto elapsed =
                                std::chrono::duration_cast<
                                    std::chrono::milliseconds>(
                                    now - cooldown->second).count();
                            inCooldown = elapsed < kRecognitionCooldownMs;
                        }

                        if (!inCooldown) {
                            const bool queued = callbacks_.submitPunch(
                                userId,
                                std::time(nullptr),
                                processFrame,
                                userName,
                                stopRequested);
                            text += queued ? " [OK]" : " [Unavailable]";
                            userCooldowns[userId] = now;
                        }

                        cv::putText(
                            processFrame,
                            text,
                            cv::Point(faceRegion.x, faceRegion.y - 10),
                            cv::FONT_HERSHEY_SIMPLEX,
                            0.9,
                            cv::Scalar(0, 255, 0),
                            2);
                    }
                }
            }

            callbacks_.publishCurrentFrame(processFrame);

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMilliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastUiUpdateTime).count();
            if (elapsedMilliseconds >= 16) {
                callbacks_.publishDisplayFrame(processFrame);
                lastUiUpdateTime = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        } catch (const cv::Exception& error) {
            std::cerr << "[Error] OpenCV Exception in capture loop: "
                      << error.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } catch (const std::exception& error) {
            std::cerr << "[Error] Std Exception in capture loop: "
                      << error.what() << std::endl;
        } catch (...) {
            std::cerr << "[Error] Unknown crash in capture loop!" << std::endl;
        }
    }

    std::cout << ">>> [Business] Stopping capture thread..." << std::endl;
    close();
    std::cout << ">>> [Business] Capture thread stopped." << std::endl;
}

void FaceCaptureWorker::close() noexcept {
    capture_.release();
}

bool FaceCaptureWorker::waitForSimulatorStream(
    const std::atomic<bool>& stopRequested) const noexcept {
    // TODO(refactor/phase-6): 将 UDP 探测和 GStreamer 管线迁入 platform/pc。
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
    address.sin_port = htons(kSimulatorStreamPort);
    if (bind(socketFd.get(), reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0) {
        return false;
    }

    pollfd descriptor{};
    descriptor.fd = socketFd.get();
    descriptor.events = POLLIN;
    while (!stopRequested.load()) {
        const int pollResult = poll(
            &descriptor, 1, kStreamProbeTimeoutMs);
        if (pollResult > 0 && (descriptor.revents & POLLIN) != 0) {
            return true;
        }
        if (pollResult > 0 &&
            (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return false;
        }
        if (pollResult < 0 && errno != EINTR) {
            return false;
        }
    }
    return false;
}

cv::VideoCapture FaceCaptureWorker::openSimulatorStream() const {
    const std::string pipeline =
        "udpsrc port=5004 timeout=2000000000 ! "
        "application/x-rtp, media=(string)video, clock-rate=(int)90000, "
        "encoding-name=(string)RAW, sampling=(string)YCbCr-4:2:2, "
        "depth=(string)8, width=(string)640, height=(string)480, "
        "colorimetry=(string)BT601-5, payload=(int)96 ! "
        "rtpjitterbuffer latency=0 ! "
        "rtpvrawdepay ! videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink sync=false drop=true max-buffers=1";
    std::cout << "[Stream] 使用硬编码管道连接..." << std::endl;
    return cv::VideoCapture(pipeline, cv::CAP_GSTREAMER);
}

} // namespace smart_attendance::business

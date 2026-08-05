#include "business/face_capture_worker.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using smart_attendance::Result;
using smart_attendance::biometric::face::FaceError;
using smart_attendance::biometric::face::FacePreprocessConfig;
using smart_attendance::biometric::face::FaceRecognition;
using smart_attendance::biometric::face::IFaceRecognitionEngine;
using smart_attendance::business::FaceCaptureWorker;
using smart_attendance::business::FaceCaptureWorkerCallbacks;
using smart_attendance::hal::CameraFrame;
using smart_attendance::hal::DeviceError;
using smart_attendance::hal::ICamera;
using smart_attendance::hal::IRtc;
using smart_attendance::hal::PixelFormat;
using smart_attendance::hal::SystemTime;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class FakeCamera final : public ICamera {
public:
    Result<void, DeviceError> open() override {
        open_ = true;
        return Result<void, DeviceError>::success();
    }

    Result<CameraFrame, DeviceError> read() override {
        ++readCalls;
        auto bytes = std::make_shared<std::vector<std::uint8_t>>(
            4U * 4U * 3U, 20U);
        std::shared_ptr<const std::uint8_t> pixels(bytes, bytes->data());
        return Result<CameraFrame, DeviceError>::success({
            std::move(pixels), bytes->size(), 4, 4, 12,
            PixelFormat::Bgr888});
    }

    bool isOpen() const noexcept override { return open_; }
    void close() noexcept override {
        open_ = false;
        ++closeCalls;
    }

    bool open_{true};
    int readCalls{0};
    int closeCalls{0};
};

class FakeRtc final : public IRtc {
public:
    Result<SystemTime, DeviceError> now() const override {
        return Result<SystemTime, DeviceError>::success({1000});
    }
    Result<void, DeviceError> set(SystemTime) override {
        return Result<void, DeviceError>::failure(DeviceError::Unsupported);
    }
    bool isWritable() const noexcept override { return false; }
};

class FakeRecognitionEngine final : public IFaceRecognitionEngine {
public:
    Result<void, FaceError> initializeDetector(const std::string&) override {
        return Result<void, FaceError>::success();
    }
    Result<std::optional<cv::Rect>, FaceError> detectLargest(
        const cv::Mat&) override {
        ++detectionCalls;
        return Result<std::optional<cv::Rect>, FaceError>::success(std::nullopt);
    }
    Result<FaceRecognition, FaceError> recognize(
        const cv::Mat&, const cv::Rect&,
        const FacePreprocessConfig&) override {
        return Result<FaceRecognition, FaceError>::failure(
            FaceError::RecognizerNotReady);
    }
    Result<cv::Mat, FaceError> toGrayscale(const cv::Mat&) override {
        return Result<cv::Mat, FaceError>::failure(FaceError::InvalidImage);
    }
    Result<void, FaceError> train(
        const std::vector<cv::Mat>&, const std::vector<int>&) override {
        return Result<void, FaceError>::success();
    }
    Result<void, FaceError> update(
        const std::vector<cv::Mat>&, const std::vector<int>&) override {
        return Result<void, FaceError>::success();
    }
    Result<void, FaceError> loadModel(const std::string&) override {
        return Result<void, FaceError>::success();
    }
    Result<void, FaceError> saveModel(const std::string&) override {
        return Result<void, FaceError>::success();
    }
    bool isTrained() const noexcept override { return false; }
    void reset() noexcept override {}

    int detectionCalls{0};
};

} // namespace

int main() {
    FakeCamera camera;
    FakeRtc rtc;
    FakeRecognitionEngine recognitionEngine;
    std::atomic<bool> stopRequested{false};
    int currentFramePublications = 0;

    FaceCaptureWorkerCallbacks callbacks;
    callbacks.recognitionEnabled = [] { return false; };
    callbacks.preprocessConfig = [] { return FacePreprocessConfig{}; };
    callbacks.resolveUserName = [](int) { return std::string("Unknown"); };
    callbacks.submitPunch = [](
        int, std::time_t, const cv::Mat&, std::string,
        const std::atomic<bool>&) { return false; };
    callbacks.publishCurrentFrame = [&](const cv::Mat& frame) {
        require(!frame.empty() && frame.cols == 4 && frame.rows == 4,
                "Worker must publish an owned OpenCV view of the HAL frame");
        ++currentFramePublications;
        stopRequested.store(true);
    };
    callbacks.publishDisplayFrame = [](const cv::Mat&) {};

    FaceCaptureWorker worker(
        recognitionEngine, camera, rtc, std::move(callbacks));
    worker.run(stopRequested);

    require(camera.readCalls == 1 && camera.closeCalls == 1,
            "Worker must read once and close the injected camera on stop");
    require(recognitionEngine.detectionCalls == 1 &&
                currentFramePublications == 2,
            "Worker must preserve raw and processed frame publication");
    std::cout << "[PASSED] capture Worker uses injected Camera HAL\n";
    return EXIT_SUCCESS;
}

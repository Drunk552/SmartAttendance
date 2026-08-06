#include "infrastructure/logging/logger.h"
/**
 * @file face_capture_worker.cpp
 * @brief 实现平台无关的摄像头采集循环并调用独立人脸算法模块。
 */

#include "business/face_capture_worker.h"

#include <chrono>
#include <iostream>
#include <map>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <utility>

namespace smart_attendance::business {

namespace {

constexpr int kSkippedFrames = 4;
constexpr int kRecognitionCooldownMs = 2000;

} // namespace

FaceCaptureWorker::FaceCaptureWorker(
    biometric::face::IFaceRecognitionEngine& recognitionEngine,
    hal::ICamera& camera,
    hal::IRtc& rtc,
    FaceCaptureWorkerCallbacks callbacks)
    : recognitionEngine_(recognitionEngine),
      camera_(camera),
      rtc_(rtc),
      callbacks_(std::move(callbacks)) {}

void FaceCaptureWorker::run(
    const std::atomic<bool>& stopRequested) {
    SA_LOG_INFO_STREAM() << ">>> [Business] Background capture thread started." << std::endl;

    int frameCounter = 0;
    cv::Rect lastFaceRegion;
    bool tracking = false;
    std::map<int, std::chrono::steady_clock::time_point> userCooldowns;
    auto lastUiUpdateTime = std::chrono::steady_clock::now();
    int consecutiveFailures = 0;
    int retryCount = 0;

    while (!stopRequested.load()) {
        try {
            if (!camera_.isOpen()) {
                if (++retryCount % 10 == 0) {
                    SA_LOG_INFO_STREAM() << "[Stream] 尝试重连 SDP..." << std::endl;
                    camera_.close();
                    if (camera_.open()) {
                        consecutiveFailures = 0;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            auto capturedFrame = camera_.read();
            if (!capturedFrame || !capturedFrame.value().isValid()) {
                ++consecutiveFailures;
                if (consecutiveFailures > 60) {
                    SA_LOG_ERROR_STREAM() << "[Stream] 严重错误：流已中断，强制重启连接！"
                              << std::endl;
                    camera_.close();
                    consecutiveFailures = 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            const auto& frame = capturedFrame.value();
            const cv::Mat frameView(
                frame.height,
                frame.width,
                CV_8UC3,
                const_cast<std::uint8_t*>(frame.data.get()),
                frame.strideBytes);
            cv::Mat processFrame = frameView.clone();
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
                            const auto systemTime = rtc_.now();
                            const bool queued = systemTime &&
                                callbacks_.submitPunch(
                                    userId,
                                    static_cast<std::time_t>(
                                        systemTime.value().unixSeconds),
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
            SA_LOG_ERROR_STREAM() << "[Error] OpenCV Exception in capture loop: "
                      << error.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } catch (const std::exception& error) {
            SA_LOG_ERROR_STREAM() << "[Error] Std Exception in capture loop: "
                      << error.what() << std::endl;
        } catch (...) {
            SA_LOG_ERROR_STREAM() << "[Error] Unknown crash in capture loop!" << std::endl;
        }
    }

    SA_LOG_INFO_STREAM() << ">>> [Business] Stopping capture thread..." << std::endl;
    close();
    SA_LOG_INFO_STREAM() << ">>> [Business] Capture thread stopped." << std::endl;
}

void FaceCaptureWorker::close() noexcept {
    camera_.close();
}

} // namespace smart_attendance::business

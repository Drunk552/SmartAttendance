/**
 * @file face_capture_worker.h
 * @brief 声明 TaskManager 所拥有线程中运行的摄像头采集与识别 Worker。
 */

#ifndef SMART_ATTENDANCE_BUSINESS_FACE_CAPTURE_WORKER_H
#define SMART_ATTENDANCE_BUSINESS_FACE_CAPTURE_WORKER_H

#include "biometric/face/face_recognition_engine.h"

#include <atomic>
#include <ctime>
#include <functional>
#include <opencv2/videoio.hpp>
#include <string>

namespace smart_attendance::business {

struct FaceCaptureWorkerCallbacks {
    std::function<bool()> recognitionEnabled;
    std::function<biometric::face::FacePreprocessConfig()> preprocessConfig;
    std::function<std::string(int)> resolveUserName;
    std::function<bool(int,
                       std::time_t,
                       const cv::Mat&,
                       std::string,
                       const std::atomic<bool>&)> submitPunch;
    std::function<void(const cv::Mat&)> publishCurrentFrame;
    std::function<void(const cv::Mat&)> publishDisplayFrame;
};

class FaceCaptureWorker final {
public:
    /**
     * @brief 创建 Worker 资源但不打开摄像头或创建线程。
     * @param recognitionEngine 生命周期必须覆盖本 Worker 及其 run 调用。
     * @param callbacks 回调目标生命周期必须覆盖 run；发布的 Mat 只在调用期间有效。
     */
    FaceCaptureWorker(
        biometric::face::IFaceRecognitionEngine& recognitionEngine,
        FaceCaptureWorkerCallbacks callbacks);

    /** @brief 在 TaskManager 所有线程中运行，观察停止标志后同步释放摄像头并返回。 */
    void run(const std::atomic<bool>& stopRequested);

    /** @brief 释放已打开的视频句柄；只能在 run 退出后由所有者调用。 */
    void close() noexcept;

private:
    bool waitForSimulatorStream(
        const std::atomic<bool>& stopRequested) const noexcept;
    cv::VideoCapture openSimulatorStream() const;

    biometric::face::IFaceRecognitionEngine& recognitionEngine_;
    FaceCaptureWorkerCallbacks callbacks_;
    cv::VideoCapture capture_;
};

} // namespace smart_attendance::business

#endif // SMART_ATTENDANCE_BUSINESS_FACE_CAPTURE_WORKER_H

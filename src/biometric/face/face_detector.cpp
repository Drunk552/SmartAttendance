/**
 * @file face_detector.cpp
 * @brief 实现旧参数兼容的 Haar 最大人脸检测。
 */

#include "biometric/face/face_detector.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace smart_attendance::biometric::face {

Result<void, FaceError> FaceDetector::load(const std::string& cascadePath) {
    using ResultType = Result<void, FaceError>;
    reset();
    if (cascadePath.empty()) {
        return ResultType::failure(FaceError::DetectorLoadFailed);
    }
    try {
        if (!cascade_.load(cascadePath)) {
            reset();
            return ResultType::failure(FaceError::DetectorLoadFailed);
        }
    } catch (const cv::Exception&) {
        reset();
        return ResultType::failure(FaceError::DetectorLoadFailed);
    }
    return ResultType::success();
}

Result<std::optional<cv::Rect>, FaceError> FaceDetector::detectLargest(
    const cv::Mat& frame) {
    using ResultType = Result<std::optional<cv::Rect>, FaceError>;
    if (frame.empty()) {
        return ResultType::failure(FaceError::InvalidImage);
    }
    if (cascade_.empty()) {
        return ResultType::failure(FaceError::DetectorNotReady);
    }

    try {
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::equalizeHist(gray, gray);
        std::vector<cv::Rect> faces;
        cascade_.detectMultiScale(
            gray, faces, 1.1, 3, 0, cv::Size(80, 80));
        if (faces.empty()) {
            return ResultType::success(std::nullopt);
        }
        const auto largest = std::max_element(
            faces.begin(), faces.end(),
            [](const cv::Rect& left, const cv::Rect& right) {
                return left.area() < right.area();
            });
        return ResultType::success(*largest);
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::DetectionFailed);
    }
}

void FaceDetector::reset() noexcept {
    cascade_ = cv::CascadeClassifier{};
}

} // namespace smart_attendance::biometric::face

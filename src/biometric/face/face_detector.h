/**
 * @file face_detector.h
 * @brief 声明 Haar 最大人脸检测器。
 */

#ifndef SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_DETECTOR_H
#define SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_DETECTOR_H

#include "biometric/face/face_types.h"
#include "core/common/result.h"

#include <opencv2/objdetect.hpp>
#include <optional>
#include <string>

namespace smart_attendance::biometric::face {

class FaceDetector final {
public:
    Result<void, FaceError> load(const std::string& cascadePath);
    Result<std::optional<cv::Rect>, FaceError> detectLargest(
        const cv::Mat& frame);
    void reset() noexcept;

private:
    cv::CascadeClassifier cascade_;
};

} // namespace smart_attendance::biometric::face

#endif // SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_DETECTOR_H

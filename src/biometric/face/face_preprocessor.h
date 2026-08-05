/**
 * @file face_preprocessor.h
 * @brief 声明无状态的人脸图像预处理器。
 */

#ifndef SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_PREPROCESSOR_H
#define SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_PREPROCESSOR_H

#include "biometric/face/face_types.h"
#include "core/common/result.h"

namespace smart_attendance::biometric::face {

class FacePreprocessor final {
public:
    Result<cv::Mat, FaceError> preprocess(
        const cv::Mat& frame,
        const cv::Rect& faceRegion,
        const FacePreprocessConfig& config) const;

    Result<cv::Mat, FaceError> toGrayscale(const cv::Mat& image) const;
};

} // namespace smart_attendance::biometric::face

#endif // SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_PREPROCESSOR_H

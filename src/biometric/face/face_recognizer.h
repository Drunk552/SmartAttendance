/**
 * @file face_recognizer.h
 * @brief 声明 LBPH 人脸识别器封装。
 */

#ifndef SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_RECOGNIZER_H
#define SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_RECOGNIZER_H

#include "biometric/face/face_types.h"
#include "core/common/result.h"

#include <opencv2/face.hpp>
#include <vector>

namespace smart_attendance::biometric::face {

class FaceModelManager;

class FaceRecognizer final {
public:
    FaceRecognizer() = default;

    Result<void, FaceError> train(
        const std::vector<cv::Mat>& samples,
        const std::vector<int>& labels);
    Result<void, FaceError> update(
        const std::vector<cv::Mat>& samples,
        const std::vector<int>& labels);
    Result<FaceRecognition, FaceError> predict(const cv::Mat& faceImage) const;

    bool isTrained() const noexcept;
    void reset() noexcept;

private:
    friend class FaceModelManager;

    bool ensureInitialized() noexcept;

    cv::Ptr<cv::face::LBPHFaceRecognizer> recognizer_;
    bool trained_{false};
};

} // namespace smart_attendance::biometric::face

#endif // SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_RECOGNIZER_H

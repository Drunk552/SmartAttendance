/**
 * @file face_types.h
 * @brief 定义人脸算法模块使用的数据和错误类型。
 */

#ifndef SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_TYPES_H
#define SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_TYPES_H

#include <opencv2/core.hpp>

namespace smart_attendance::biometric::face {

enum class HistogramEqualization {
    None = 0,
    Global = 1,
    Clahe = 2
};

struct FacePreprocessConfig {
    bool cropEnabled{false};
    int cropMarginPercent{0};
    bool resizeAndEqualizeEnabled{false};
    bool resizeEnabled{false};
    cv::Size resizeSize{};
    HistogramEqualization histogramEqualization{HistogramEqualization::None};
    float claheClipLimit{0.0F};
    cv::Size claheTileGridSize{};
    bool roiEnhancementEnabled{false};
    float roiContrast{0.0F};
    float roiBrightness{0.0F};
};

struct FaceRecognition {
    int userId{-1};
    double confidence{0.0};
};

enum class FaceError {
    InvalidImage,
    InvalidFaceRegion,
    DetectorNotReady,
    DetectorLoadFailed,
    DetectionFailed,
    RecognizerNotReady,
    TrainingDataInvalid,
    TrainingFailed,
    RecognitionFailed,
    ModelReadFailed,
    ModelWriteFailed
};

} // namespace smart_attendance::biometric::face

#endif // SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_TYPES_H

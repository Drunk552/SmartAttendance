/**
 * @file face_model_manager.h
 * @brief 声明人脸模型文件读写职责。
 */

#ifndef SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_MODEL_MANAGER_H
#define SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_MODEL_MANAGER_H

#include "biometric/face/face_types.h"
#include "core/common/result.h"

#include <string>

namespace smart_attendance::biometric::face {

class FaceRecognizer;

class FaceModelManager final {
public:
    Result<void, FaceError> load(
        const std::string& modelPath,
        FaceRecognizer& recognizer) const;
    Result<void, FaceError> save(
        const std::string& modelPath,
        const FaceRecognizer& recognizer) const;
};

} // namespace smart_attendance::biometric::face

#endif // SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_MODEL_MANAGER_H

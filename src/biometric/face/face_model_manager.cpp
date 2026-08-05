/**
 * @file face_model_manager.cpp
 * @brief 实现 LBPH 模型持久化边界。
 */

#include "biometric/face/face_model_manager.h"

#include "biometric/face/face_recognizer.h"

namespace smart_attendance::biometric::face {

Result<void, FaceError> FaceModelManager::load(
    const std::string& modelPath,
    FaceRecognizer& recognizer) const {
    using ResultType = Result<void, FaceError>;
    if (modelPath.empty()) {
        return ResultType::failure(FaceError::ModelReadFailed);
    }
    if (!recognizer.ensureInitialized()) {
        return ResultType::failure(FaceError::RecognizerNotReady);
    }
    try {
        recognizer.recognizer_->read(modelPath);
        recognizer.trained_ = true;
        return ResultType::success();
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::ModelReadFailed);
    }
}

Result<void, FaceError> FaceModelManager::save(
    const std::string& modelPath,
    const FaceRecognizer& recognizer) const {
    using ResultType = Result<void, FaceError>;
    if (modelPath.empty() || !recognizer.trained_ ||
        recognizer.recognizer_.empty()) {
        return ResultType::failure(FaceError::ModelWriteFailed);
    }
    try {
        recognizer.recognizer_->write(modelPath);
        return ResultType::success();
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::ModelWriteFailed);
    }
}

} // namespace smart_attendance::biometric::face

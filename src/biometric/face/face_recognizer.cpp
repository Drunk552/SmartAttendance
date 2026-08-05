/**
 * @file face_recognizer.cpp
 * @brief 实现参数与旧模块一致的 LBPH 识别器。
 */

#include "biometric/face/face_recognizer.h"

namespace smart_attendance::biometric::face {

namespace {

bool validTrainingData(const std::vector<cv::Mat>& samples,
                       const std::vector<int>& labels) {
    if (samples.empty() || samples.size() != labels.size()) {
        return false;
    }
    for (const auto& sample : samples) {
        if (sample.empty()) {
            return false;
        }
    }
    return true;
}

} // namespace

Result<void, FaceError> FaceRecognizer::train(
    const std::vector<cv::Mat>& samples,
    const std::vector<int>& labels) {
    using ResultType = Result<void, FaceError>;
    if (!validTrainingData(samples, labels)) {
        return ResultType::failure(FaceError::TrainingDataInvalid);
    }
    if (!ensureInitialized()) {
        return ResultType::failure(FaceError::RecognizerNotReady);
    }
    try {
        recognizer_->train(samples, labels);
        trained_ = true;
        return ResultType::success();
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::TrainingFailed);
    }
}

Result<void, FaceError> FaceRecognizer::update(
    const std::vector<cv::Mat>& samples,
    const std::vector<int>& labels) {
    using ResultType = Result<void, FaceError>;
    if (!validTrainingData(samples, labels)) {
        return ResultType::failure(FaceError::TrainingDataInvalid);
    }
    if (!trained_) {
        return train(samples, labels);
    }
    try {
        recognizer_->update(samples, labels);
        return ResultType::success();
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::TrainingFailed);
    }
}

Result<FaceRecognition, FaceError> FaceRecognizer::predict(
    const cv::Mat& faceImage) const {
    using ResultType = Result<FaceRecognition, FaceError>;
    if (faceImage.empty()) {
        return ResultType::failure(FaceError::InvalidImage);
    }
    if (!trained_ || recognizer_.empty()) {
        return ResultType::failure(FaceError::RecognizerNotReady);
    }

    try {
        FaceRecognition recognition;
        recognizer_->predict(
            faceImage, recognition.userId, recognition.confidence);
        return ResultType::success(recognition);
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::RecognitionFailed);
    }
}

bool FaceRecognizer::isTrained() const noexcept {
    return trained_;
}

void FaceRecognizer::reset() noexcept {
    recognizer_.release();
    trained_ = false;
}

bool FaceRecognizer::ensureInitialized() noexcept {
    if (!recognizer_.empty()) {
        return true;
    }
    try {
        recognizer_ = cv::face::LBPHFaceRecognizer::create(
            1, 8, 8, 8, 500.0);
        return !recognizer_.empty();
    } catch (...) {
        recognizer_.release();
        return false;
    }
}

} // namespace smart_attendance::biometric::face

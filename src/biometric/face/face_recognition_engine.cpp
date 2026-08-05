/**
 * @file face_recognition_engine.cpp
 * @brief 组合检测、预处理、识别和模型管理对象。
 */

#include "biometric/face/face_recognition_engine.h"

namespace smart_attendance::biometric::face {

Result<void, FaceError> FaceRecognitionEngine::initializeDetector(
    const std::string& cascadePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    return detector_.load(cascadePath);
}

Result<std::optional<cv::Rect>, FaceError>
FaceRecognitionEngine::detectLargest(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    return detector_.detectLargest(frame);
}

Result<FaceRecognition, FaceError> FaceRecognitionEngine::recognize(
    const cv::Mat& frame,
    const cv::Rect& faceRegion,
    const FacePreprocessConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto preprocessed = preprocessor_.preprocess(frame, faceRegion, config);
    if (!preprocessed) {
        return Result<FaceRecognition, FaceError>::failure(
            preprocessed.error());
    }
    return recognizer_.predict(preprocessed.value());
}

Result<cv::Mat, FaceError> FaceRecognitionEngine::toGrayscale(
    const cv::Mat& image) {
    return preprocessor_.toGrayscale(image);
}

Result<void, FaceError> FaceRecognitionEngine::train(
    const std::vector<cv::Mat>& samples,
    const std::vector<int>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    return recognizer_.train(samples, labels);
}

Result<void, FaceError> FaceRecognitionEngine::update(
    const std::vector<cv::Mat>& samples,
    const std::vector<int>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    return recognizer_.update(samples, labels);
}

Result<void, FaceError> FaceRecognitionEngine::loadModel(
    const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelManager_.load(modelPath, recognizer_);
}

Result<void, FaceError> FaceRecognitionEngine::saveModel(
    const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelManager_.save(modelPath, recognizer_);
}

bool FaceRecognitionEngine::isTrained() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return recognizer_.isTrained();
}

void FaceRecognitionEngine::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    detector_.reset();
    recognizer_.reset();
}

} // namespace smart_attendance::biometric::face

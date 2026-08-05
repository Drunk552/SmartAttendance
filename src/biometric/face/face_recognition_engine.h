/**
 * @file face_recognition_engine.h
 * @brief 声明人脸算法抽象和默认 OpenCV 组合实现。
 */

#ifndef SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_RECOGNITION_ENGINE_H
#define SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_RECOGNITION_ENGINE_H

#include "biometric/face/face_detector.h"
#include "biometric/face/face_model_manager.h"
#include "biometric/face/face_preprocessor.h"
#include "biometric/face/face_recognizer.h"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace smart_attendance::biometric::face {

/**
 * @brief OpenCV 人脸算法边界，不负责摄像头、线程、数据库或 UI。
 *
 * 输入图像只在调用期间读取；返回的 Mat 拥有独立数据。实现必须把第三方异常转换
 * 为 FaceError。除 isTrained 外的方法由调用线程同步执行，可能包含图像计算或文件
 * I/O，不能在 LVGL 渲染临界区调用。
 */
class IFaceRecognitionEngine {
public:
    virtual ~IFaceRecognitionEngine() = default;

    virtual Result<void, FaceError> initializeDetector(
        const std::string& cascadePath) = 0;
    virtual Result<std::optional<cv::Rect>, FaceError> detectLargest(
        const cv::Mat& frame) = 0;
    virtual Result<FaceRecognition, FaceError> recognize(
        const cv::Mat& frame,
        const cv::Rect& faceRegion,
        const FacePreprocessConfig& config) = 0;
    virtual Result<cv::Mat, FaceError> toGrayscale(
        const cv::Mat& image) = 0;
    virtual Result<void, FaceError> train(
        const std::vector<cv::Mat>& samples,
        const std::vector<int>& labels) = 0;
    virtual Result<void, FaceError> update(
        const std::vector<cv::Mat>& samples,
        const std::vector<int>& labels) = 0;
    virtual Result<void, FaceError> loadModel(
        const std::string& modelPath) = 0;
    virtual Result<void, FaceError> saveModel(
        const std::string& modelPath) = 0;
    virtual bool isTrained() const noexcept = 0;
    virtual void reset() noexcept = 0;
};

/**
 * @brief 组合 Haar、预处理、LBPH 与模型文件管理的默认实现。
 * @note 内部串行保护模型，允许采集 Worker 与 UI 注册流程安全共享。
 */
class FaceRecognitionEngine final : public IFaceRecognitionEngine {
public:
    Result<void, FaceError> initializeDetector(
        const std::string& cascadePath) override;
    Result<std::optional<cv::Rect>, FaceError> detectLargest(
        const cv::Mat& frame) override;
    Result<FaceRecognition, FaceError> recognize(
        const cv::Mat& frame,
        const cv::Rect& faceRegion,
        const FacePreprocessConfig& config) override;
    Result<cv::Mat, FaceError> toGrayscale(
        const cv::Mat& image) override;
    Result<void, FaceError> train(
        const std::vector<cv::Mat>& samples,
        const std::vector<int>& labels) override;
    Result<void, FaceError> update(
        const std::vector<cv::Mat>& samples,
        const std::vector<int>& labels) override;
    Result<void, FaceError> loadModel(
        const std::string& modelPath) override;
    Result<void, FaceError> saveModel(
        const std::string& modelPath) override;
    bool isTrained() const noexcept override;
    void reset() noexcept override;

private:
    mutable std::mutex mutex_;
    FaceDetector detector_;
    FacePreprocessor preprocessor_;
    FaceRecognizer recognizer_;
    FaceModelManager modelManager_;
};

} // namespace smart_attendance::biometric::face

#endif // SMART_ATTENDANCE_BIOMETRIC_FACE_FACE_RECOGNITION_ENGINE_H

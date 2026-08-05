/**
 * @file face_preprocessor.cpp
 * @brief 实现与旧 face_demo 相同行为的人脸预处理流程。
 */

#include "biometric/face/face_preprocessor.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

namespace smart_attendance::biometric::face {

namespace {

cv::Mat equalize(const cv::Mat& image, const FacePreprocessConfig& config) {
    cv::Mat result;
    if (config.histogramEqualization == HistogramEqualization::Global) {
        cv::equalizeHist(image, result);
    } else {
        // 阶段五只做职责迁移：旧实现中的 CLAHE 分支不可达，因此继续保持克隆行为。
        // TODO(refactor/post-phase-5): 通过独立算法变更和图像基线测试启用 CLAHE。
        result = image.clone();
    }
    return result;
}

cv::Mat enhance(const cv::Mat& image, const FacePreprocessConfig& config) {
    if (!config.roiEnhancementEnabled) {
        return image.clone();
    }

    cv::Mat enhanced;
    image.convertTo(enhanced, -1, config.roiContrast, config.roiBrightness);
    cv::threshold(enhanced, enhanced, 255, 255, cv::THRESH_TRUNC);
    cv::threshold(enhanced, enhanced, 0, 0, cv::THRESH_TOZERO);
    return enhanced;
}

} // namespace

Result<cv::Mat, FaceError> FacePreprocessor::preprocess(
    const cv::Mat& frame,
    const cv::Rect& faceRegion,
    const FacePreprocessConfig& config) const {
    using ResultType = Result<cv::Mat, FaceError>;
    if (frame.empty()) {
        return ResultType::failure(FaceError::InvalidImage);
    }

    const cv::Rect frameBounds(0, 0, frame.cols, frame.rows);
    if (faceRegion.width <= 0 || faceRegion.height <= 0 ||
        (faceRegion & frameBounds) != faceRegion) {
        return ResultType::failure(FaceError::InvalidFaceRegion);
    }

    try {
        cv::Mat crop = frame(faceRegion).clone();
        auto grayscaleResult = toGrayscale(crop);
        if (!grayscaleResult) {
            return ResultType::failure(grayscaleResult.error());
        }
        cv::Mat gray = std::move(grayscaleResult).value();

        if (config.cropEnabled) {
            const int marginX = std::max(
                0, crop.cols * config.cropMarginPercent / 100);
            const int marginY = std::max(
                0, crop.rows * config.cropMarginPercent / 100);
            const cv::Rect tight(
                marginX,
                marginY,
                std::max(1, crop.cols - 2 * marginX),
                std::max(1, crop.rows - 2 * marginY));
            gray = gray(tight & cv::Rect(0, 0, crop.cols, crop.rows));
        }

        if (config.resizeAndEqualizeEnabled) {
            if (config.resizeEnabled) {
                cv::resize(gray, gray, config.resizeSize);
            }
            gray = equalize(gray, config);
        }

        return ResultType::success(enhance(gray, config));
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::InvalidFaceRegion);
    }
}

Result<cv::Mat, FaceError> FacePreprocessor::toGrayscale(
    const cv::Mat& image) const {
    using ResultType = Result<cv::Mat, FaceError>;
    if (image.empty()) {
        return ResultType::failure(FaceError::InvalidImage);
    }

    try {
        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
        } else if (image.channels() == 1) {
            image.copyTo(gray);
        } else {
            return ResultType::failure(FaceError::InvalidImage);
        }
        return ResultType::success(std::move(gray));
    } catch (const cv::Exception&) {
        return ResultType::failure(FaceError::InvalidImage);
    }
}

} // namespace smart_attendance::biometric::face

#include "biometric/face/face_preprocessor.h"
#include "biometric/face/face_recognition_engine.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using smart_attendance::biometric::face::FaceError;
using smart_attendance::biometric::face::FacePreprocessConfig;
using smart_attendance::biometric::face::FacePreprocessor;
using smart_attendance::biometric::face::FaceRecognitionEngine;
using smart_attendance::biometric::face::HistogramEqualization;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

cv::Mat makeSample() {
    cv::Mat sample(64, 64, CV_8UC1, cv::Scalar(30));
    cv::circle(sample, cv::Point(20, 22), 7, cv::Scalar(220), -1);
    cv::circle(sample, cv::Point(44, 22), 7, cv::Scalar(220), -1);
    cv::rectangle(sample, cv::Rect(18, 43, 28, 5), cv::Scalar(180), -1);
    return sample;
}

void testPreprocessorBoundariesAndCompatibility() {
    FacePreprocessor preprocessor;
    const auto emptyImage = preprocessor.toGrayscale(cv::Mat());
    require(!emptyImage && emptyImage.error() == FaceError::InvalidImage,
            "empty image must return InvalidImage");

    cv::Mat bgr(40, 50, CV_8UC3, cv::Scalar(10, 20, 30));
    auto invalidRegion = preprocessor.preprocess(
        bgr, cv::Rect(45, 0, 10, 10), FacePreprocessConfig{});
    require(!invalidRegion &&
                invalidRegion.error() == FaceError::InvalidFaceRegion,
            "out-of-range face region must be rejected");

    FacePreprocessConfig config;
    config.resizeAndEqualizeEnabled = true;
    config.resizeEnabled = true;
    config.resizeSize = cv::Size(20, 16);
    config.histogramEqualization = HistogramEqualization::Global;
    auto processed = preprocessor.preprocess(
        bgr, cv::Rect(5, 5, 30, 25), config);
    require(processed && processed.value().channels() == 1 &&
                processed.value().size() == config.resizeSize,
            "preprocessor must crop, grayscale and resize independently");

    config.histogramEqualization = HistogramEqualization::Clahe;
    auto claheCompatibility = preprocessor.preprocess(
        bgr, cv::Rect(5, 5, 30, 25), config);
    require(claheCompatibility &&
                claheCompatibility.value().size() == config.resizeSize,
            "phase-five CLAHE compatibility path must remain usable");
}

void testTrainingPredictionAndModelLifecycle() {
    FaceRecognitionEngine engine;
    const auto badDetector = engine.initializeDetector("/path/does/not/exist.xml");
    require(!badDetector &&
                badDetector.error() == FaceError::DetectorLoadFailed,
            "missing cascade must report DetectorLoadFailed");

    const cv::Mat sample = makeSample();
    const auto invalidTraining = engine.train({sample}, {});
    require(!invalidTraining &&
                invalidTraining.error() == FaceError::TrainingDataInvalid,
            "mismatched labels must be rejected");
    require(engine.train({sample}, {7}).hasValue(),
            "valid grayscale sample must train LBPH");
    require(engine.isTrained(), "successful training must set trained state");

    cv::Mat bgr;
    cv::cvtColor(sample, bgr, cv::COLOR_GRAY2BGR);
    auto recognition = engine.recognize(
        bgr, cv::Rect(0, 0, bgr.cols, bgr.rows), FacePreprocessConfig{});
    require(recognition && recognition.value().userId == 7,
            "trained engine must recognize its source sample");

    const std::filesystem::path modelPath =
        std::filesystem::temp_directory_path() /
        ("smartattendance_face_model_" + std::to_string(::getpid()) + ".xml");
    std::error_code removeError;
    std::filesystem::remove(modelPath, removeError);
    require(engine.saveModel(modelPath.string()).hasValue(),
            "trained model must be persisted");

    FaceRecognitionEngine loadedEngine;
    require(loadedEngine.loadModel(modelPath.string()) &&
                loadedEngine.isTrained(),
            "persisted model must load into a fresh engine");
    auto loadedRecognition = loadedEngine.recognize(
        bgr, cv::Rect(0, 0, bgr.cols, bgr.rows), FacePreprocessConfig{});
    require(loadedRecognition && loadedRecognition.value().userId == 7,
            "loaded model must preserve recognition labels");
    std::filesystem::remove(modelPath, removeError);
}

} // namespace

int main() {
    testPreprocessorBoundariesAndCompatibility();
    testTrainingPredictionAndModelLifecycle();
    std::cout << "[PASSED] face recognition module boundaries\n";
    return EXIT_SUCCESS;
}

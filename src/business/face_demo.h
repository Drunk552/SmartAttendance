// src/business/face_detect.h
#ifndef FACE_DEMO_H
#define FACE_DEMO_H

#include "db_storage.h"//数据层头文件

#ifdef __cplusplus
extern "C" {
#endif

bool business_init();// 初始化函数
bool business_processAndSaveImage(const cv::Mat& inputImage);//请求业务层处理并保存函数声明
bool test_business_processAndSaveImage(const std::string& imagePath);//测试business_processAndSaveImage接口声明
bool test_convertToGrayscale(const std::string& imagePath);//测试convertToGrayscale函数声明


// 单次运行函数
void business_run_once();

cv::Mat convertToGrayscale(const cv::Mat& inputImage);

#ifdef __cplusplus
}
#endif

#endif // FACE_DEMO_H
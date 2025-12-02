// src/business/face_demo.h
#pragma once
#include <opencv2/core.hpp>

// 初始化函数
bool business_init();

// 保存图像数据的函数声明（使用 C++ cv::Mat 类型）
bool data_saveImage(const cv::Mat &image);

// 单次运行函数
void business_run_once();

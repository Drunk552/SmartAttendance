// src/business/face_detect.h
#ifndef FACE_DEMO_H
#define FACE_DEMO_H

// 只有 C++ 编译器能看到的区域
#ifdef __cplusplus
#include "db_storage.h"//数据层头文件
#include <opencv2/core.hpp> // 包含 cv::Mat 定义,只有 C++ 编译器才引入 OpenCV
#endif

//C 语言兼容接口声明
#ifdef __cplusplus
extern "C" {
#endif

bool business_init();// 初始化函数
bool business_capture_snapshot();// 触发拍照函数


// ==========================================
// Epic 4 新增接口
// ==========================================

/**
 * @brief 获取当前视频帧用于 UI 显示
 * * 将当前最新的摄像头帧转换为 LVGL 支持的颜色格式 (RGB24/32) 并填充到 buffer 中。
 * * @param[out] buffer 接收图像数据的内存指针 (由 UI 层分配)
 * @param[in]  w      期望的宽度 (如 240)
 * @param[in]  h      期望的高度 (如 180，留点空间给按钮)
 * @return true 获取成功, false 失败
 */
bool business_get_display_frame(void* buffer, int w, int h);

/**
 * @brief 触发拍照动作
 * * 捕获当前帧，进行灰度处理，并保存到数据库。
 * * @return true 成功, false 失败
 */
bool business_capture_snapshot();

// ==========================================
// C++ 专用接口 (C 文件不可见)
// ==========================================

#ifdef __cplusplus
}
#endif

//只有 C++ 内部使用的函数声明 (放在 extern "C" 外面)
#ifdef __cplusplus
bool business_processAndSaveImage(const cv::Mat& inputImage);//请求业务层处理并保存函数声明
cv::Mat convertToGrayscale(const cv::Mat& inputImage);
#endif

#endif // FACE_DEMO_H
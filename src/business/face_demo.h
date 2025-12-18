/**
 * @file face_demo.h
 * @brief 人脸识别演示模块的头文件  
 * 定义业务层对外接口，供 UI 层调用
 * @note 仅包含 C++ 代码，UI 层通过 extern "C" 调用
 */

// src/business/face_detect.h
#ifndef FACE_DEMO_H
#define FACE_DEMO_H

#include "db_storage.h"//数据层头文件

// 只有 C++ 编译器能看到的区域
#ifdef __cplusplus
#include <opencv2/core.hpp> // 包含 cv::Mat 定义,只有 C++ 编译器才引入 OpenCV

typedef cv::Size CvSizeCompat; // C++用 OpenCV 的 cv::Size 类型

#else
// 只有 C 编译器能看到的区域
typedef struct {
    int width;  // 对应cv::Size的width
    int height; // 对应cv::Size的height
} CvSizeCompat; // C用自定义的兼容类型

#endif

//C 语言兼容接口声明
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 业务层初始化函数
 * @return bool - 初始化是否成功
 */
bool business_init();// 初始化函数

/**
 * @brief 业务单次运行函数（Epic 4 修改版）
 * @return cv::Mat 处理后的图像（带有人脸框和文字），用于 UI 显示
 */ 
bool business_capture_snapshot();// 触发拍照函数

typedef enum {
    HIST_EQ_NONE = 0,     // 禁用
    HIST_EQ_GLOBAL = 1,   // 全局均衡化
    HIST_EQ_CLAHE = 2     // CLAHE自适应均衡化
} HistEqMethod;// 直方图均衡化方法枚举

/**
 * @brief 人脸预处理配置结构体
 * @note 包含裁剪边界、尺寸归一化等选项
 */
typedef struct {
    bool enable_crop;                 // 是否裁剪边界
    int crop_margin_percent;          // 裁剪百分比
    bool enable_resize_eq;            // 是否尺寸归一化 + 直方图均衡化
    bool enablez_resize;              // 是否调整尺寸
    CvSizeCompat resize_size;         // 兼容类型：C用自定义，C++用cv::Size
    
    // 直方图均衡化方法
    int hist_eq_method;               // 0=无, 1=全局, 2=CLAHE
    
    // CLAHE参数
    float clahe_clip_limit;           // CLAHE剪切限制
    CvSizeCompat clahe_tile_grid_size; // CLAHE网格大小

    // ROI处理参数
    bool enable_roi_enhance;          // 是否增强ROI对比度
    float roi_contrast;               // ROI对比度增强因子
    float roi_brightness;             // ROI亮度增强偏移量

    // 调试选项
    bool debug_show_steps;            // 是否显示调试中间步骤
} PreprocessConfig;


// 配置接口
void business_set_preprocess_config(const PreprocessConfig* config);// 设置预处理配置

PreprocessConfig business_get_preprocess_config(void);// 获取当前预处理配置
void business_set_histogram_equalization(bool enable, int method);// 设置直方图均衡化选项
void business_set_crop_settings(bool enable, int margin_percent);// 设置裁剪选项（UI）
void business_set_clahe_parameters(float clip_limit, int grid_width, int grid_height);// 设置CLAHE参数（UI）
void business_set_roi_enhance(bool enable, float contrast, float brightness);// 设置ROI增强参数

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

/**
 * @brief 请求业务层处理并保存图像
 * @param inputImage - 从摄像头捕获的原始图像（BGR格式，与OpenCV一致）
 * @return bool - 处理及保存是否成功
 * @note 严格遵循接口定义：UI层 -> 业务层
 */
bool business_processAndSaveImage(const cv::Mat& inputImage);//请求业务层处理并保存函数声明

/**
 * @brief 将BGR图像转换为灰度图像
 * @param inputImage 输入图像（BGR或BGRA格式）
 * @return 灰度图像
 * @note Epic 3要求实现的独立函数
 */
cv::Mat convertToGrayscale(const cv::Mat& inputImage);
#endif

#endif // FACE_DEMO_H
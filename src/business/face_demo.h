/**
 * @file face_demo.h
 * @brief 业务层接口定义
 * @details 提供人脸检测、识别、训练以及视频流获取的核心业务逻辑。
 * @author SmartAttendance Team
 * @version 1.1 (Epic 5 Update)
 */
#ifndef FACE_DEMO_H
#define FACE_DEMO_H

// 只有 C++ 编译器能看到的区域
#ifdef __cplusplus

#include "db_storage.h"//数据层头文件

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
 * @brief 初始化业务模块
 * @details 加载人脸检测模型、初始化识别器、打开摄像头或视频流。
 * @return true 初始化成功
 * @return false 初始化失败 (如模型文件丢失、摄像头无法打开)
 */
bool business_init();

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
void business_reload_config();// 强制刷新考勤配置 (供 UI 设置保存后调用)


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
 * @brief 触发一次快照采集
 * @details 捕获当前帧，检测人脸，预处理并保存到数据库。
 * @return true 采集并保存成功
 * @return false 失败 (未检测到人脸或数据库错误)
 */
bool business_capture_snapshot();

// ==========================================
// Epic 3.3 新增: 用户列表数据接口
// ==========================================

/**
 * @brief 获取当前数据库中的用户总数
 */
int business_get_user_count(void);

/**
 * @brief 获取指定索引的用户信息 (用于 C 语言 UI 显示)
 * @param index 列表索引 (0 ~ count-1)
 * @param id_out 输出: 用户 ID
 * @param name_buf 输出: 名字缓冲区
 * @param len 缓冲区大小
 * @return true 获取成功, false 索引越界
 */
bool business_get_user_at(int index, int *id_out, char *name_buf, int len);

/**
 * @brief 注册新用户 (Register New User)
 * @details 捕获当前的摄像头画面作为人脸特征，结合输入的用户名创建新用户并存入数据库。
 * 若注册成功，会自动刷新内部的用户列表缓存。
 * * @param name 待注册的用户名 (C 字符串)
 * @return true  注册成功
 * @return false 注册失败 (可能原因：当前无视频帧、数据库写入失败等)
 */
bool business_register_user(const char* name);

// ==========================================
// 考勤记录查询接口 (Data Model for UI List)
// ==========================================

/**
 * @brief 刷新/加载考勤记录缓存
 * @details 从数据库查询最近的考勤记录并缓存到业务层内存中。
 * 建议在进入“考勤记录”界面时调用一次。
 * @return int 加载到的记录数量
 */
int business_load_records();

/**
 * @brief 获取当前缓存的考勤记录总数
 * @details 用于通知 UI 列表控件（如 lv_table 或 lv_list）需要渲染多少行。
 * @return int 记录条数
 */
int business_get_record_count(void);

/**
 * @brief 获取指定索引的考勤记录格式化文本
 * @details 将考勤记录（时间、姓名、状态）格式化为字符串，供 UI 显示。
 * 格式示例: "2023-12-14 08:30 | 张三 | 正常"
 * * @param[in]  index 列表索引 (0 ~ count-1)
 * @param[out] buf   目标缓冲区，用于接收格式化后的字符串
 * @param[in]  len   缓冲区大小 (建议 >= 64 字节)
 * @return true  获取成功
 * @return false 索引越界或缓冲区无效
 */
bool business_get_record_at(int index, char *buf, int len);

// ==========================================
// C++ 专用接口 (C 文件不可见)
// ==========================================

#ifdef __cplusplus
}
#endif

//只有 C++ 内部使用的函数声明 (放在 extern "C" 外面)
#ifdef __cplusplus

#include <opencv2/core.hpp>

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
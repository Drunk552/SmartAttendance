#ifndef UI_APP_H
#define UI_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_app.h
 * @brief UI层核心接口（C/C++兼容）- 负责LVGL界面渲染、摄像头预览与交互
 * @note 依赖LVGL和SDL2初始化完成，窗口尺寸默认240x320（适配团队框架）
 * @details 包含UI初始化、摄像头预览更新、打卡结果提示等接口，支持C和C++调用
 * @author 黄霖
 * @version 1.1
 * @date 2025-12-03
 */

// 原有接口（保留，不修改）
void ui_init(void);

/**
 * @brief 更新摄像头预览画面
 * @details 将采集模块的图像帧渲染到LVGL预览组件，保证50-60FPS
 * @param[in] frame 摄像头帧（C风格指针，实际传递cv::Mat的地址，需在C++中转换）
 * @note 主线程调用，图像尺寸建议640x480
 */
void ui_update_camera_preview(const void* frame);

/**
 * @brief 显示打卡结果提示
 * @param[in] msg 提示文本（C字符串）
 * @param[in] is_success 1=成功（绿色），0=失败（红色）
 */
void ui_show_attendance_msg(const char* msg, int is_success);

#ifdef __cplusplus
}
#endif

#endif // UI_APP_H
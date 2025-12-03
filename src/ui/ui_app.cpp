#include "ui_app.h"
// 绝对路径包含lvgl.h（解决路径报错，后续可按CMake配置调整）
#include "/home/alice/work/SmartAttendance/libs/lvgl/third_party/lvgl/src/lvgl.h"
#include <SDL2/SDL.h>
#include <opencv2/opencv.hpp>  // 修复cv::Mat未定义
#include <string>              // 修复std::string未定义
#include <iostream>            // 模拟接口需要打印日志
#include "/home/alice/work/SmartAttendance/libs/lvgl/third_party/lvgl/src/misc/lv_timer.h"  // 修复lv_timer_t未定义

// -------------------------- 【临时模拟接口】- 仅用于UI层独立测试，后续删除 --------------------------
// 说明：模拟采集层和业务层接口，同伴提供真实接口后，直接删除该区域所有代码
// 1. 模拟采集层：get_current_frame() - 返回本地图片/纯色图作为摄像头帧
cv::Mat get_current_frame() {
    // 可选：替换为你本地测试图路径（如项目根目录的test.jpg），不存在则生成蓝色纯色图
    cv::Mat mock_frame = cv::imread("/home/alice/work/SmartAttendance/test.jpg");
    if (mock_frame.empty()) {
        mock_frame = cv::Mat(480, 640, CV_8UC3, cv::Scalar(255, 0, 0)); // 640x480蓝色帧
    }
    return mock_frame;
}

// 2. 模拟业务层：business_processAndSaveImage() - 打印日志+保存图片，模拟接口调用
bool business_processAndSaveImage(const cv::Mat& inputImage) {
    // 模拟业务层处理逻辑，仅打印日志（同伴实现真实逻辑后删除）
    std::cout << "[UI层-模拟调用] 成功传递图像到业务层，尺寸：" << inputImage.cols << "x" << inputImage.rows << std::endl;
    // 保存捕获的帧到项目根目录，验证传递成功（后续删除）
    cv::imwrite("/home/alice/work/SmartAttendance/ui_test_captured.jpg", inputImage);
    return true; // 模拟处理成功
}

// 3. 模拟工具函数：mat_to_lv_img() - cv::Mat转LVGL图像格式（解决预览渲染报错）
lv_img_dsc_t mat_to_lv_img(const cv::Mat& mat) {
    static lv_img_dsc_t img_dsc;
    cv::Mat rgb_mat;
    // 强制转换为BGR565格式（用OpenCV原生函数，避免宏）
    mat.convertTo(rgb_mat, CV_8UC2);
    cv::cvtColor(mat, rgb_mat, cv::COLOR_BGR2BGR565);
    
    img_dsc.header.w = mat.cols;
    img_dsc.header.h = mat.rows;
    img_dsc.header.cf = 1; // 若仍报错，直接写1
    img_dsc.data_size = rgb_mat.total() * rgb_mat.elemSize();
    img_dsc.data = (uint8_t*)rgb_mat.data;
    return img_dsc;
}
// -------------------------- 【临时模拟接口结束】--------------------------

// UI层内部全局变量（仅本文件使用，不对外暴露）
static lv_obj_t* camera_preview;  // 摄像头预览组件
static lv_obj_t* capture_btn;     // 捕获按钮

// 捕获按钮回调函数（点击触发图像捕获+接口调用）
static void capture_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // 1. 调用采集层接口（模拟）获取当前帧
        cv::Mat current_frame = get_current_frame();
        if (current_frame.empty()) {
            ui_show_attendance_msg("摄像头帧获取失败", 0); // 0=失败（红色）
            return;
        }

        // 2. 调用业务层接口（模拟）传递捕获的帧
        bool ret = business_processAndSaveImage(current_frame);
        if (ret) {
            ui_show_attendance_msg("图像已捕获并保存", 1); // 1=成功（绿色）
        } else {
            ui_show_attendance_msg("保存失败，请重试", 0);
        }
    }
}

// 原有接口实现：UI初始化（创建窗口+控件+绑定事件）
void ui_init(void) {
    // 临时跳过 LVGL/SDL2 初始化（仅验证逻辑）
    std::cout << "[临时UI层] 跳过图形初始化，直接验证逻辑" << std::endl;

    // 模拟按钮点击的回调绑定（仅测试接口）
    std::cout << "[临时UI层] 已绑定捕获按钮回调" << std::endl;

    // 自动触发一次捕获（验证接口调用）
    cv::Mat frame = get_current_frame();
    bool ret = business_processAndSaveImage(frame);
    if (ret) {
        std::cout << "[临时UI层] 模拟捕获成功，生成 ui_test_captured.jpg" << std::endl;
    } else {
        std::cout << "[临时UI层] 模拟捕获失败" << std::endl;
    }


    // 2. 创建LVGL主窗口（800x480，嵌入式常用尺寸）
    lv_obj_t* main_win = lv_obj_create(NULL);
    lv_scr_load(main_win);
    lv_obj_set_size(main_win, 800, 480);

    // 3. 创建摄像头预览组件（上半部分，居中显示）
    camera_preview = lv_img_create(main_win);
    lv_obj_set_size(camera_preview, 760, 320); // 预览区域尺寸
    lv_obj_align(camera_preview, LV_ALIGN_TOP_MID, 0, 20); // 上边缘间距20px

    // 4. 创建捕获按钮（下半部分，居中显示）
    capture_btn = lv_btn_create(main_win);
    lv_obj_set_size(capture_btn, 160, 60); // 按钮尺寸
    lv_obj_align(capture_btn, LV_ALIGN_BOTTOM_MID, 0, -40); // 下边缘间距40px
    lv_obj_add_event_cb(capture_btn, capture_btn_cb, LV_EVENT_ALL, NULL); // 绑定回调

    // 5. 按钮文本（中文支持，依赖项目字体配置）
    lv_obj_t* btn_label = lv_label_create(capture_btn);
    lv_label_set_text(btn_label, "捕获图像");
    lv_obj_center(btn_label);
}

// 接口实现：更新摄像头预览画面
void ui_update_camera_preview(const void* frame) {
    if (frame == NULL) return; // 防止空指针崩溃

    // 将C风格void*转换为C++的cv::Mat（安全转换）
    const cv::Mat& mat_frame = *static_cast<const cv::Mat*>(frame);
    // 转换为LVGL图像格式并渲染
    lv_img_dsc_t lv_img = mat_to_lv_img(mat_frame);
    lv_img_set_src(camera_preview, &lv_img);
}

// 接口实现：显示打卡结果提示框
void ui_show_attendance_msg(const char* msg, int is_success) {
    if (msg == NULL) msg = "未知提示";

    // 放弃msgbox，直接用label+背景框模拟提示（适配所有LVGL版本）
    lv_obj_t* bg_box = lv_obj_create(NULL);
    lv_obj_set_size(bg_box, 300, 150);
    lv_obj_center(bg_box);

    // 设置提示文本
    lv_obj_t* text_label = lv_label_create(bg_box);
    lv_label_set_text(text_label, msg);
    lv_obj_center(text_label);

    // 设置文本颜色（成功绿色/失败红色）
    lv_color_t text_color = is_success ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
    lv_obj_set_style_text_color(text_label, text_color, 0);

    // 用SDL_Delay+手动删除替代LVGL定时器（彻底绕开版本问题）
    // 注意：仅测试用，后续整合时需替换为LVGL的定时接口
    SDL_Delay(3000);
    lv_obj_del(bg_box);
}

// -------------------------- 【临时测试入口】- 仅用于UI层独立运行，后续删除 --------------------------
// 说明：同伴整合时，删除该main函数（项目主入口在src/main.cpp）

// -------------------------- 【临时测试入口结束】--------------------------
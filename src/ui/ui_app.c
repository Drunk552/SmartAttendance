/**
 * @file ui_app.c
 * @brief UI 层主程序 - Phase 03 竖屏适配版
 * @details 实现 Epic 3.1 竖屏框架搭建与视频层缩放适配
 * @version 1.2 (Phase 03 - Epic 3.1)
 */
#include "ui_app.h"
#include <lvgl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "../business/face_demo.h" // 引入业务接口
#include "lv_conf.h"

// ================= 全局变量与配置 =================
#define SCREEN_W 240
#define SCREEN_H 320

// [Epic 3.1] 摄像头预览区尺寸适配 240x180 (4:3)
#define CAM_W 240
#define CAM_H 180

static lv_obj_t *main_screen; // 主待机界面

// 样式定义
static lv_style_t style_screen_portrait; // 全局竖屏样式
static lv_style_t style_text_title;      // 标题/状态栏文字
static lv_style_t style_text_info;       // 信息区文字

// 字体指针 (需在 init_fonts 中加载)
static lv_font_t *g_font_zh = NULL;

// 摄像头相关
static uint8_t cam_buf[CAM_W * CAM_H * 3]; // RGB888 缓冲区
static lv_obj_t *img_camera = NULL;        // 图像控件
static lv_image_dsc_t img_dsc;             // 图像描述符

// UI 组件句柄 (用于后续更新内容)
static lv_obj_t *label_time;    // 顶部时间
static lv_obj_t *label_status;  // 底部状态提示
static lv_obj_t *label_result;  // 识别结果 (姓名/工号)

// ================= 辅助函数 =================

// 获取当前时间字符串 (HH:MM)
static void get_current_time_str(char *buf, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buf, size, "%H:%M", timeinfo);
}

// 定时器：刷新摄像头画面 (30FPS)
static void camera_timer_cb(lv_timer_t *timer) {
    if (!img_camera) return;

    // [Epic 3.1 User Story 2] 调用业务层获取缩放后的帧 (240x180)
    // 业务层 face_demo.cpp 中的 business_get_display_frame 会处理缩放逻辑
    if (business_get_display_frame(cam_buf, CAM_W, CAM_H)) {
        lv_obj_invalidate(img_camera); // 通知 LVGL 重绘
    }
}

// 定时器：刷新顶部时间 (每秒)
static void time_timer_cb(lv_timer_t *timer) {
    if (label_time) {
        char buf[16];
        get_current_time_str(buf, sizeof(buf));
        lv_label_set_text(label_time, buf);
    }
}

// ================= 样式初始化 =================
static void init_styles(void) {
    // 1. 竖屏全局样式
    lv_style_init(&style_screen_portrait);
    lv_style_set_bg_color(&style_screen_portrait, lv_color_black()); // 黑色背景，突出视频
    lv_style_set_bg_opa(&style_screen_portrait, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen_portrait, lv_color_white());

    // 2. 文本样式
    lv_style_init(&style_text_title);
    lv_style_set_text_font(&style_text_title, LV_FONT_DEFAULT); // 默认字体用于数字/英文

    lv_style_init(&style_text_info);
    // 如果加载了中文字体则使用，否则回退默认
#if LV_USE_FREETYPE
    // 这里保留原有的 FreeType 加载逻辑，但在本示例中简化处理
#endif
    if (g_font_zh) {
        lv_style_set_text_font(&style_text_info, g_font_zh);
    } else {
        lv_style_set_text_font(&style_text_info, LV_FONT_DEFAULT);
    }
}

// ================= 界面构建 (Epic 3.1 Layout) =================

static void create_main_ui(void) {
    main_screen = lv_obj_create(NULL);
    lv_obj_add_style(main_screen, &style_screen_portrait, 0);
    lv_obj_set_scrollbar_mode(main_screen, LV_SCROLLBAR_MODE_OFF);

    // --- 1. 顶部状态栏 (Top Bar) Height: 30px ---
    lv_obj_t *top_bar = lv_obj_create(main_screen);
    lv_obj_set_size(top_bar, SCREEN_W, 30);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0); // 透明背景
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 5, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 时间显示
    label_time = lv_label_create(top_bar);
    lv_label_set_text(label_time, "00:00");
    lv_obj_add_style(label_time, &style_text_title, 0);
    lv_obj_align(label_time, LV_ALIGN_LEFT_MID, 5, 0);

    // WiFi 图标 (模拟)
    lv_obj_t *label_wifi = lv_label_create(top_bar);
    lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
    lv_obj_align(label_wifi, LV_ALIGN_RIGHT_MID, -5, 0);

    // --- 2. 摄像头预览区 (Video Area) 240x180 ---
    // 文档要求: y=40 (给顶部留出空间)
    
    // 初始化图像描述符
    img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
    img_dsc.header.w = CAM_W;
    img_dsc.header.h = CAM_H;
    img_dsc.header.stride = CAM_W * 3;
    img_dsc.data = cam_buf;
    img_dsc.data_size = sizeof(cam_buf);

    img_camera = lv_image_create(main_screen);
    lv_image_set_src(img_camera, &img_dsc);
    lv_obj_set_size(img_camera, CAM_W, CAM_H);
    lv_obj_align(img_camera, LV_ALIGN_TOP_MID, 0, 40); // 居中，向下偏移40px
    
    // 添加边框以区分边界
    lv_obj_set_style_border_width(img_camera, 1, 0);
    lv_obj_set_style_border_color(img_camera, lv_palette_main(LV_PALETTE_GREY), 0);

    // --- 3. 底部信息区 (Bottom Info) Height: ~110px ---
    lv_obj_t *bottom_info = lv_obj_create(main_screen);
    lv_obj_set_size(bottom_info, SCREEN_W, 110);
    lv_obj_align(bottom_info, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_info, lv_color_hex(0x222222), 0); // 深灰色底
    lv_obj_set_style_border_width(bottom_info, 0, 0);
    lv_obj_set_flex_flow(bottom_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottom_info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 识别结果/欢迎语
    label_result = lv_label_create(bottom_info);
    lv_label_set_text(label_result, "请正对摄像头");
    lv_obj_add_style(label_result, &style_text_info, 0);
    lv_obj_set_style_text_font(label_result, &lv_font_montserrat_20, 0); // 使用较大字体

    // 操作提示
    label_status = lv_label_create(bottom_info);
    lv_label_set_text(label_status, "按 [MENU] 键设置");
    lv_obj_add_style(label_status, &style_text_info, 0);
    lv_obj_set_style_text_color(label_status, lv_palette_main(LV_PALETTE_YELLOW), 0);

    // --- 4. 启动定时器 ---
    lv_timer_create(camera_timer_cb, 33, NULL); // ~30FPS 刷新视频
    lv_timer_create(time_timer_cb, 1000, NULL); // 1s 刷新时间
    
    lv_screen_load(main_screen);
}

// ================= 初始化入口 =================

void ui_init(void)
{
    // 1. 初始化 LVGL 核心
    lv_init();

    // [Epic 3.1 User Story 1] 设置竖屏分辨率 240x320
    lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
    (void)disp;
    
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    // 2. 初始化资源
    init_styles();

    // 3. 构建界面
    create_main_ui();

    printf("UI: Phase 03 Portrait Mode Initialized. (240x320)\n");
}
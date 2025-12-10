/**
 * @file ui_app.c
 * @brief UI 层主程序 - 优化版 (Epic 5)
 * @details 适配 800x480 分辨率，采用左右分栏布局，模拟真实考勤机界面。
 */
#include"ui_app.h"
#include <lvgl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include"../business/face_demo.h"//引入业务接口
#include "lv_conf.h"

#if LV_USE_FREETYPE
#include "lv_freetype.h"
#endif

// ================= 配置宏 =================
// 模拟 7英寸嵌入式屏幕分辨率
#define WIN_W 800
#define WIN_H 480

// [Epic4新增] 摄像头显示相关的变量
#define CAM_W 320
#define CAM_H 240

// ================= 全局变量 =================
static lv_font_t *g_font_zh_large = NULL; // 大字体 (标题)
static lv_font_t *g_font_zh_normal = NULL; // 普通字体
static lv_style_t st_title;
static lv_style_t st_body;
static lv_style_t st_symbol;

#define SCREEN_COUNT 3
static lv_obj_t *screens[SCREEN_COUNT];
static uint32_t current_screen_idx = 0;

static lv_font_t *g_font_zh = NULL;
static lv_style_t st_body;   /* 中文正文字体 */
//static lv_style_t st_symbol; /* 符号/图标字体（内置默认） 


// 图像缓冲区: 宽 * 高 * 3字节(RGB888)
static uint8_t cam_buf[CAM_W * CAM_H * 3]; 
static lv_obj_t *img_camera = NULL;// 保存定时器句柄
static lv_image_dsc_t img_dsc; // LVGL 图像描述符
static lv_obj_t *lbl_status_log = NULL;// [新增这一行] 定义日志标签变量

// ================= 辅助宏 =================
#define INDEX_TO_PTR(i) ((void *)(uintptr_t)(i))
#define PTR_TO_INDEX(p) ((uint32_t)(uintptr_t)(p))

// 辅助函数：在界面标签上显示日志
static void ui_log(const char *format, ...) {
    if (!lbl_status_log) return;

    char buf[128];

    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    lv_label_set_text(lbl_status_log, buf);
}

// ================= 事件回调 =================
// 在按钮回调中使用
static void capture_btn_cb(lv_event_t *e) {
    // 1. 先显示日志，给用户反馈
    ui_log("正在采集..."); // 实时反馈

    // 2. 强制刷新 UI，让文字立即显示
    lv_timer_handler();

    // 3. 调用业务接口
    if (business_capture_snapshot()) {
        ui_log("采集成功! ID已入库");
    }else{
        ui_log("采集失败: 无人脸");
    }
}

// [Epic4新增] 定时器回调：刷新摄像头画面
static void camera_timer_cb(lv_timer_t *timer) {
    if (!img_camera) return;

    // 1. 从业务层获取最新一帧数据
    if (business_get_display_frame(cam_buf, CAM_W, CAM_H)) {
        // 2. 通知 LVGL 图片源数据变了，需要重绘
        lv_obj_invalidate(img_camera);
    }
}

/* ===== 辅助：动画回调，尽量用 translate，避免触发布局重排 ===== */
static void anim_translate_x_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)obj, v, 0);
}
static void anim_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

/* ===== 事件 ===== */
static void btn_event_cb(lv_event_t *e)
{
    static uint32_t cnt = 0;
    lv_obj_t *label = lv_event_get_user_data(e);
    cnt++;
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Clicked %u", (unsigned)cnt);
    lv_label_set_text(label, buf);
}

// ================= 导航栏与页面切换 =================

static void switch_screen_event_cb(lv_event_t *e)
{
    uint32_t target_idx = PTR_TO_INDEX(lv_event_get_user_data(e));
    if (target_idx >= SCREEN_COUNT || target_idx == current_screen_idx)
        return;
    lv_scr_load_anim_t anim = (target_idx > current_screen_idx) ? LV_SCR_LOAD_ANIM_MOVE_LEFT
                                                                : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    current_screen_idx = target_idx;
    lv_screen_load_anim(screens[current_screen_idx], anim, 260, 0, false);
}

static void update_slider_label(lv_obj_t *slider, lv_obj_t *label)
{
    if (!slider || !label)
        return;
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)lv_slider_get_value(slider));
    lv_label_set_text(label, buf);
}
static void slider_value_changed_cb(lv_event_t *e)
{
    update_slider_label(lv_event_get_target(e), lv_event_get_user_data(e));
}

/* ===== 底部导航 ===== */
static void add_navbar(lv_obj_t *root_scr)
{
    lv_obj_t *bar = lv_obj_create(root_scr);
    lv_obj_set_scroll_dir(bar, LV_DIR_NONE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 40);
    lv_obj_set_style_bg_opa(bar, LV_OPA_80, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *names[3] = {"Home", "List", "Ctrl"};
    for (uint8_t i = 0; i < 3; ++i)
    {
        lv_obj_t *btn = lv_button_create(bar);
        lv_obj_set_size(btn, 68, 32);
        lv_obj_add_event_cb(btn, switch_screen_event_cb, LV_EVENT_CLICKED, INDEX_TO_PTR(i));
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, names[i]);
        /* 导航文字用中文字体： */
        if (g_font_zh)
            lv_obj_add_style(label, &st_body, 0);
        lv_obj_center(label);
    }
}

// ================= 页面 1: 主页 (操作台) =================

// [Epic4修改] create_main_screen: 添加摄像头显示
/* ===== 三个界面 ===== */
/* 根容器统一：列布局（内容 + 导航），根不滚动；内容区用 flex_grow(1) 填满剩余高度 */
static void create_main_ui(void) {
    // 1. 获取当前屏幕
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_ROW); // 左右布局
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 20, 0);

    // --- 左侧：摄像头预览 ---
    lv_obj_t *panel_left = lv_obj_create(scr);
    lv_obj_set_size(panel_left, CAM_W + 40, WIN_H - 40);
    lv_obj_set_flex_flow(panel_left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *lbl_title = lv_label_create(panel_left);
    lv_label_set_text(lbl_title, "人脸考勤终端");
    if (g_font_zh_large) lv_obj_add_style(lbl_title, &st_title, 0);
    
    // 初始化摄像头图像
    memset(cam_buf, 0x80, sizeof(cam_buf));
    img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
    img_dsc.header.w = CAM_W;
    img_dsc.header.h = CAM_H;
    img_dsc.header.stride = CAM_W * 3;
    img_dsc.data = cam_buf;
    img_dsc.data_size = sizeof(cam_buf);
    
    img_camera = lv_image_create(panel_left);
    lv_image_set_src(img_camera, &img_dsc);
    lv_obj_set_style_radius(img_camera, 4, 0);

    // 状态日志标签
    lbl_status_log = lv_label_create(panel_left);
    lv_label_set_text(lbl_status_log, "系统就绪");
    if (g_font_zh_normal) lv_obj_add_style(lbl_status_log, &st_body, 0);

    // --- 右侧：操作区 ---
    lv_obj_t *panel_right = lv_obj_create(scr);
    lv_obj_set_flex_grow(panel_right, 1);
    lv_obj_set_height(panel_right, WIN_H - 40);
    lv_obj_set_flex_flow(panel_right, LV_FLEX_FLOW_COLUMN);
    
    // 拍照按钮
    lv_obj_t *btn_cap = lv_button_create(panel_right);
    lv_obj_set_width(btn_cap, lv_pct(80));
    lv_obj_set_height(btn_cap, 60);
    lv_obj_add_event_cb(btn_cap, capture_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl_btn = lv_label_create(btn_cap);
    lv_label_set_text(lbl_btn, "打卡 / 采集");
    if (g_font_zh_large) lv_obj_add_style(lbl_btn, &st_title, 0);
    lv_obj_center(lbl_btn);

    // 启动定时器
    lv_timer_create(camera_timer_cb, 33, NULL);
}

/* 关键：不用 lv_list，自己做一个可滚动面板 + 行按钮，图标使用内置默认字体 */
static lv_obj_t *create_list_screen(uint32_t idx)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 顶部标题（根不滚动） */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "任务列表");
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(title, &st_body, 0);

    /* 唯一滚动层：panel 自己滚动，外层不滚动；隐藏滚动条避免宽度变动 */
    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(panel, 4, 0);

    /* 固定高的行，文本不换行，避免测量重入 */
    const struct
    {
        const char *icon;
        const char *txt;
    } items[] = {
        {LV_SYMBOL_OK, "测试1"},
        {LV_SYMBOL_EDIT, "测试2"},
        {LV_SYMBOL_REFRESH, "测试3"},
        {LV_SYMBOL_UPLOAD, "测试4"},
        {LV_SYMBOL_DOWNLOAD, "测试5"},
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i)
    {
        lv_obj_t *row = lv_button_create(panel);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);                     /* 关键：固定高度 */
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE); /* 行自身不可滚 */
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_set_style_pad_ver(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *icon = lv_label_create(row);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_add_style(icon, &st_symbol, 0); /* 符号用内置字体 */

        lv_obj_t *txt = lv_label_create(row);
        lv_label_set_text(txt, items[i].txt);
        lv_obj_add_style(txt, &st_body, 0);              /* 中文用 FreeType 字体 */
        lv_label_set_long_mode(txt, LV_LABEL_LONG_CLIP); /* 不换行，防止测量重入 */
        lv_obj_set_width(txt, lv_pct(100));              /* 占满剩余宽度 */
    }

    add_navbar(scr);
    return scr;
}

// ================= 页面 3: 控制页  =================

static lv_obj_t *create_controls_screen(uint32_t idx)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_all(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "参数调节");
    if (g_font_zh)
        lv_obj_add_style(title, &st_body, 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *slider = lv_slider_create(content);
    lv_obj_set_width(slider, lv_pct(100));
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);

    lv_obj_t *value_label = lv_label_create(content);
    if (g_font_zh)
        lv_obj_add_style(value_label, &st_body, 0);
    lv_obj_add_event_cb(slider, slider_value_changed_cb, LV_EVENT_VALUE_CHANGED, value_label);
    update_slider_label(slider, value_label);

    add_navbar(scr);
    return scr;
}

/* ===== 字体初始化：只给正文中文字体；符号用默认字体 ===== */
static void init_fonts(void)
{
#if LV_USE_FREETYPE
    if (lv_freetype_init(256) == LV_RESULT_OK)
    {
        printf("lv_freetype_init failed\n");
        return;
    }
    const char *cands[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/mnt/c/Windows/Fonts/msyh.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        NULL};
    const char *path = NULL;
    for (int i = 0; cands[i]; ++i)
        if (access(cands[i], R_OK) == 0)
        {
            path = cands[i];
            break;
        }
    if (path)
    {
        g_font_zh = lv_freetype_font_create(path,LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 14, LV_FREETYPE_FONT_STYLE_NORMAL);
        g_font_zh_large = lv_freetype_font_create(path,LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 24, LV_FREETYPE_FONT_STYLE_NORMAL);
        g_font_zh_normal = lv_freetype_font_create(path,LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16, LV_FREETYPE_FONT_STYLE_NORMAL);
        // 【新增这行】将 normal 字体同时也赋值给 g_font_zh，修复 "add_navbar" 中的报错
        g_font_zh = g_font_zh_normal;
    }   
#endif
    lv_style_init(&st_body);

    lv_style_init(&st_body);
    if (g_font_zh_normal) // 改为 g_font_zh_normal
        lv_style_set_text_font(&st_body, g_font_zh_normal);

    lv_style_init(&st_title);
    if (g_font_zh_large)  // 改为 g_font_zh_large
        lv_style_set_text_font(&st_title, g_font_zh_large);

    lv_style_init(&st_symbol);
    lv_style_set_text_font(&st_symbol, LV_FONT_DEFAULT); /* 内置默认字体，含符号 */
}

/* ===== main ===== */
//[修改1]将main改为ui_init,供外部调用
void ui_init(void)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(WIN_W, WIN_H);
    (void)disp;
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    init_fonts(); /* 先准备字体样式，再创建界面，逐个给到需要的对象 */

    //screens[0] = create_main_screen(0);
    screens[0] = lv_obj_create(NULL);      // 【可选】给个空对象防止数组越界访问崩溃
    screens[1] = create_list_screen(1);
    screens[2] = create_controls_screen(2);

    current_screen_idx = 0;
    //lv_screen_load(screens[current_screen_idx]);

    create_main_ui();

    printf("UI: Init done. Camera buffer: %d bytes\n", (int)sizeof(cam_buf));

    //[修改2]删除了原本这里的while(1)循环
    //这里的任务只是“初始化”，干完活就返回，让主程序继续往下跑
    /*
    while (1)
    {
        uint32_t ms = lv_timer_handler();
        usleep(ms * 1000);
    }
    return 0;
    */
}

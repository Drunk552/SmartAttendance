#include <lvgl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/videoio/videoio_c.h>
#include "lv_conf.h"

#if LV_USE_FREETYPE
#include "lv_freetype.h"
#endif

/* ===================== 原有代码常量与全局变量 ===================== */
#define SCREEN_COUNT 4  // 新增摄像头屏幕，总数变为4
static lv_obj_t *screens[SCREEN_COUNT];
static uint32_t current_screen_idx = 0;
static lv_font_t *g_font_zh = NULL;
static lv_style_t st_body; /* 中文正文字体 */
static lv_style_t st_symbol; /* 符号/图标字体（内置默认） */
#define INDEX_TO_PTR(i) ((void *)(uintptr_t)(i))
#define PTR_TO_INDEX(p) ((uint32_t)(uintptr_t)(p))

/* ===================== 摄像头功能全局变量（模块化封装） ===================== */
static lv_obj_t* camera_preview;  // 摄像头预览组件
static lv_obj_t* capture_btn;     // 捕获按钮
static CvCapture* cap;            // 外置摄像头捕获对象（OpenCV C接口）
static bool camera_init_ok = false; // 摄像头初始化状态
static lv_timer_t* preview_timer; // LVGL预览更新定时器


// ========== 函数原型声明（加在所有函数定义前） ==========
static void preview_timer_cb(lv_timer_t* timer);
static void bgr888_to_rgb565(const IplImage* bgr, uint16_t** rgb565_out, int* size_out);
static lv_image_dsc_t iplimage_to_lv_img(const IplImage* img);
static void capture_btn_cb(lv_event_t* e);
void ui_show_attendance_msg(const char* msg, int is_success);


/* ===================== 原有代码：动画回调函数 ===================== */
static void anim_translate_x_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)obj, v, 0);
}

static void anim_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

/* ===================== 原有代码：按钮与屏幕切换事件 ===================== */
static void btn_event_cb(lv_event_t *e)
{
    static uint32_t cnt = 0;
    lv_obj_t *label = (lv_obj_t*)lv_event_get_user_data(e);
    cnt++;
    char buf[48];
    lv_snprintf(buf, sizeof(buf), "Clicked %u", (unsigned)cnt);
    lv_label_set_text(label, buf);
}

static void switch_screen_event_cb(lv_event_t *e)
{
    uint32_t target_idx = PTR_TO_INDEX(lv_event_get_user_data(e));
    if (target_idx >= SCREEN_COUNT || target_idx == current_screen_idx)
        return;
    
    // 切换到摄像头屏幕时，确保摄像头已初始化
    if (target_idx == 3) {
        if (!camera_init_ok) {
            cap = cvCaptureFromCAM(0);
            if (cap != NULL) {
                cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_WIDTH, 640);
                cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_HEIGHT, 480);
                cvSetCaptureProperty(cap, CV_CAP_PROP_FPS, 30);
                camera_init_ok = true;
                preview_timer = lv_timer_create(preview_timer_cb, 33, NULL);
            }
        }
    } else {
        // 离开摄像头屏幕时，暂停摄像头预览
        if (camera_init_ok && preview_timer != NULL) {
            lv_timer_pause(preview_timer);
        }
    }
    
    lv_scr_load_anim_t anim = (target_idx > current_screen_idx) ?
        LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    current_screen_idx = target_idx;
    lv_screen_load_anim(screens[current_screen_idx], anim, 260, 0, false);
}

/* ===================== 原有代码：滑块与标签更新 ===================== */
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
    update_slider_label((lv_obj_t*)lv_event_get_target(e), (lv_obj_t*)lv_event_get_user_data(e));
}

/* ===================== 原有代码：底部导航栏 ===================== */
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
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *names[SCREEN_COUNT] = {"Home", "List", "Ctrl", "Camera"}; // 新增摄像头导航项
    for (uint8_t i = 0; i < SCREEN_COUNT; ++i)
    {
        lv_obj_t *btn = lv_button_create(bar);
        lv_obj_set_size(btn, 68, 32);
        lv_obj_add_event_cb(btn, switch_screen_event_cb, LV_EVENT_CLICKED, INDEX_TO_PTR(i));
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, names[i]);
        if (g_font_zh)
            lv_obj_add_style(label, &st_body, 0);
        lv_obj_center(label);
    }
}

/* ===================== 原有代码：主屏幕创建 ===================== */
static lv_obj_t *create_main_screen(uint32_t idx)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_all(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "LVGL 中文测试");
    if (g_font_zh)
        lv_obj_add_style(title, &st_body, 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t *btn = lv_button_create(content);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "点击我");
    if (g_font_zh)
        lv_obj_add_style(lbl, &st_body, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, lbl);
    
    lv_obj_update_layout(content);
    lv_coord_t w_content = lv_obj_get_width(content);
    lv_coord_t btn_w = lv_obj_get_width(btn);
    lv_coord_t span = w_content - btn_w - 16;
    if (span < 0) span = 0;
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_values(&a, 0, span);
    lv_anim_set_time(&a, 1200);
    lv_anim_set_playback_time(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_translate_x_cb);
    lv_anim_start(&a);
    
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, title);
    lv_anim_set_values(&a2, 80, 255);
    lv_anim_set_time(&a2, 800);
    lv_anim_set_playback_time(&a2, 800);
    lv_anim_set_repeat_count(&a2, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a2, lv_anim_path_linear);
    lv_anim_set_exec_cb(&a2, anim_opa_cb);
    lv_anim_start(&a2);
    
    add_navbar(scr);
    return scr;
}

/* ===================== 原有代码：列表屏幕创建 ===================== */
static lv_obj_t *create_list_screen(uint32_t idx)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "任务列表");
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(title, &st_body, 0);
    
    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(panel, 4, 0);
    
    const struct {
        const char *icon;
        const char *txt;
    } items[] = {
        {LV_SYMBOL_OK, "测试1"},
        {LV_SYMBOL_EDIT, "测试2"},
        {LV_SYMBOL_REFRESH, "测试3"},
        {LV_SYMBOL_UPLOAD, "测试4"},
        {LV_SYMBOL_DOWNLOAD, "测试5"},
    };
    
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); ++i) {
        lv_obj_t *row = lv_button_create(panel);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_set_style_pad_ver(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        
        lv_obj_t *icon = lv_label_create(row);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_add_style(icon, &st_symbol, 0);
        
        lv_obj_t *txt = lv_label_create(row);
        lv_label_set_text(txt, items[i].txt);
        lv_obj_add_style(txt, &st_body, 0);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(txt, lv_pct(100));
    }
    
    add_navbar(scr);
    return scr;
}

/* ===================== 原有代码：控制屏幕创建 ===================== */
static lv_obj_t *create_controls_screen(uint32_t idx)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_all(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
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
    lv_obj_add_event_cb(slider, slider_value_changed_cb,
                        LV_EVENT_VALUE_CHANGED, value_label);
    update_slider_label(slider, value_label);
    
    add_navbar(scr);
    return scr;
}

/* ===================== 摄像头功能：BGR888转RGB565 ===================== */
static void bgr888_to_rgb565(const IplImage* bgr, uint16_t** rgb565_out, int* size_out)
{
    if (bgr == NULL) {
        *rgb565_out = NULL;
        *size_out = 0;
        return;
    }
    
    int size = bgr->width * bgr->height;
    uint16_t* rgb565 = (uint16_t*)malloc(size * sizeof(uint16_t));
    if (rgb565 == NULL) {
        *rgb565_out = NULL;
        *size_out = 0;
        return;
    }
    
    int idx = 0;
    for (int i = 0; i < bgr->height; i++) {
        const uchar* bgr_ptr = (const uchar*)(bgr->imageData + i * bgr->widthStep);
        for (int j = 0; j < bgr->width; j++) {
            uchar b = bgr_ptr[3*j];
            uchar g = bgr_ptr[3*j+1];
            uchar r = bgr_ptr[3*j+2];
            
            uint16_t r5 = (r >> 3) & 0x1F;
            uint16_t g6 = (g >> 2) & 0x3F;
            uint16_t b5 = (b >> 3) & 0x1F;
            uint16_t rgb565_val = (r5 << 11) | (g6 << 5) | b5;
            
            rgb565[idx++] = rgb565_val;
        }
    }
    
    *rgb565_out = rgb565;
    *size_out = size;
}

/* ===================== 摄像头功能：IplImage转LVGL图像描述符 ===================== */
static lv_image_dsc_t iplimage_to_lv_img(const IplImage* img)
{
    lv_image_dsc_t img_dsc = {0};
    if (img == NULL) return img_dsc;
    
    uint16_t* rgb565_data = NULL;
    int rgb565_size = 0;
    bgr888_to_rgb565(img, &rgb565_data, &rgb565_size);
    
    if (rgb565_data == NULL) return img_dsc;
    
    img_dsc.header.w = img->width;
    img_dsc.header.h = img->height;
    img_dsc.header.cf = 0x03; // LV_IMAGE_CF_RGB565
    
    img_dsc.data_size = rgb565_size * sizeof(uint16_t);
    img_dsc.data = (uint8_t*)malloc(img_dsc.data_size);
    if (img_dsc.data != NULL) {
       memcpy((void*)img_dsc.data, (const void*)rgb565_data, img_dsc.data_size);
    }
    free(rgb565_data);
    return img_dsc;
}

/* ===================== 摄像头功能：预览定时器回调 ===================== */
static void preview_timer_cb(lv_timer_t* timer)
{
    if (!camera_init_ok) return;
    
    IplImage* frame = cvQueryFrame(cap);
    if (frame == NULL) {
        fprintf(stderr, "[UI层警告] 摄像头帧读取失败！\n");
        return;
    }
    
    static lv_image_dsc_t old_img_dsc = {0};
    if (old_img_dsc.data != NULL) {
        free((void*)old_img_dsc.data);
        old_img_dsc.data = NULL;
        old_img_dsc.data_size = 0;
    }
    
    old_img_dsc = iplimage_to_lv_img(frame);
    lv_img_set_src(camera_preview, &old_img_dsc);
}

/* ===================== 摄像头功能：捕获按钮回调 ===================== */
static void capture_btn_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!camera_init_ok) {
        ui_show_attendance_msg("摄像头未初始化", 0);
        return;
    }
    
    IplImage* current_frame = cvQueryFrame(cap);
    if (current_frame == NULL) {
        ui_show_attendance_msg("帧捕获失败", 0);
        return;
    }
    
    // 业务层函数声明（需与业务层对接）
    bool ret = business_processAndSaveImage(current_frame);
    
    if (ret) {
        ui_show_attendance_msg("图像捕获并保存成功", 1);
    } else {
        ui_show_attendance_msg("图像保存失败", 0);
    }
}

{
    if (msg == NULL) msg = "未知提示";
    
    lv_obj_t* bg_box = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg_box, 300, 150);
    lv_obj_center(bg_box);
    
    lv_obj_t* text_label = lv_label_create(bg_box);
    lv_label_set_text(text_label, msg);
    lv_obj_center(text_label);
    
    lv_color_t text_color = is_success ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
    lv_obj_set_style_text_color(text_label, text_color, 0);
    if (g_font_zh) lv_obj_add_style(text_label, &st_body, 0);
    
    lv_timer_t* timer = lv_timer_create(NULL, 3000, bg_box);
    lv_timer_set_cb(timer, (lv_timer_cb_t)lv_obj_del);
}

/* ===================== 摄像头功能：摄像头屏幕创建 ===================== */
static lv_obj_t *create_camera_screen(uint32_t idx)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    
    // 摄像头预览组件
    camera_preview = lv_img_create(scr);
    lv_obj_set_size(camera_preview, 760, 320);
    lv_obj_set_flex_grow(camera_preview, 1);
    lv_obj_align(camera_preview, LV_ALIGN_TOP_MID, 0, 20);
    
    // 捕获按钮
    capture_btn = lv_button_create(scr);
    lv_obj_set_size(capture_btn, 160, 60);
    lv_obj_align(capture_btn, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(capture_btn, capture_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* btn_label = lv_label_create(capture_btn);
    lv_label_set_text(btn_label, "捕获图像");
    if (g_font_zh) lv_obj_add_style(btn_label, &st_body, 0);
    lv_obj_center(btn_label);
    
    add_navbar(scr);
    return scr;
}

/* ===================== 原有代码：字体初始化 ===================== */
static void init_fonts(void)
{
#if LV_USE_FREETYPE
    if (lv_freetype_init(256) != LV_RESULT_OK) {
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
        if (access(cands[i], R_OK) == 0) {
            path = cands[i];
            break;
        }
    
    if (path) {
        g_font_zh = lv_freetype_font_create(path,
                    LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 14, LV_FREETYPE_FONT_STYLE_NORMAL);
    }
#endif
    
    lv_style_init(&st_body);
    if (g_font_zh)
        lv_style_set_text_font(&st_body, g_font_zh);
    
    lv_style_init(&st_symbol);
    lv_style_set_text_font(&st_symbol, LV_FONT_DEFAULT);
}

/* ===================== 全局UI初始化（整合所有功能） ===================== */
void ui_init(void)
{
    // 1. LVGL核心初始化
    lv_init();
    
    // 2. SDL2初始化（复用原有SDL2配置）
    SDL_Init(SDL_INIT_VIDEO);
    lv_sdl_window_create(800, 480);
    
    // 3. 字体初始化
    init_fonts();
    
    // 4. 创建所有屏幕
    screens[0] = create_main_screen(0);
    screens[1] = create_list_screen(1);
    screens[2] = create_controls_screen(2);
    screens[3] = create_camera_screen(3);
    
    // 5. 加载初始屏幕
    lv_screen_load(screens[0]);
}

/* ===================== 摄像头资源释放 ===================== */
void ui_deinit(void)
{
    if (camera_init_ok) {
        if (preview_timer != NULL) {
            lv_timer_del(preview_timer);
            preview_timer = NULL;
        }
        cvReleaseCapture(&cap);
        camera_init_ok = false;
    }
    lv_deinit();
    SDL_Quit();
}
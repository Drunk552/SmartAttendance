#include <lvgl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lv_conf.h"
#if LV_USE_FREETYPE
#include "lv_freetype.h"
#endif
#define SCREEN_COUNT 3
static lv_obj_t *screens[SCREEN_COUNT];
static uint32_t current_screen_idx = 0;
static lv_font_t *g_font_zh = NULL;
static lv_style_t st_body; /* 中文正文字体 */
static lv_style_t st_symbol; /* 符号/图标字体（内置默认） */
#define INDEX_TO_PTR(i) ((void *)(uintptr_t)(i))
#define PTR_TO_INDEX(p) ((uint32_t)(uintptr_t)(p))
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
static void switch_screen_event_cb(lv_event_t *e)
{
uint32_t target_idx = PTR_TO_INDEX(lv_event_get_user_data(e));
if (target_idx >= SCREEN_COUNT || target_idx == current_screen_idx)
return;
lv_scr_load_anim_t anim = (target_idx > current_screen_idx) ?
LV_SCR_LOAD_ANIM_MOVE_LEFT
:
LV_SCR_LOAD_ANIM_MOVE_RIGHT;
current_screen_idx = target_idx;
lv_screen_load_anim(screens[current_screen_idx], anim, 260, 0, false);
}
static void update_slider_label(lv_obj_t *slider, lv_obj_t *label)
{
if (!slider || !label)
return;
char buf[32];
lv_snprintf(buf, sizeof(buf), "Brightness: %d%%",
(int)lv_slider_get_value(slider));
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
lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
const char *names[3] = {"Home", "List", "Ctrl"};
for (uint8_t i = 0; i < 3; ++i)
{
lv_obj_t *btn = lv_button_create(bar);
lv_obj_set_size(btn, 68, 32);
lv_obj_add_event_cb(btn, switch_screen_event_cb, LV_EVENT_CLICKED,
INDEX_TO_PTR(i));
lv_obj_t *label = lv_label_create(btn);
lv_label_set_text(label, names[i]);
/* 导航文字用中文字体： */
if (g_font_zh)
lv_obj_add_style(label, &st_body, 0);
lv_obj_center(label);
}
}
/* ===== 三个界面 ===== */
/* 根容器统一：列布局（内容 + 导航），根不滚动；内容区用 flex_grow(1) 填满剩余高度 */
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
/* 使用 translate_x 做动画，避免触发布局 */
lv_obj_update_layout(content);
lv_coord_t w_content = lv_obj_get_width(content);
lv_coord_t btn_w = lv_obj_get_width(btn);
lv_coord_t span = w_content - btn_w - 16;
if (span < 0)
span = 0;
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
/* 关键：不用 lv_list，自己做一个可滚动面板 + 行按钮，图标使用内置默认字体 */
static lv_obj_t *create_list_screen(uint32_t idx)
{
lv_obj_t *scr = lv_obj_create(NULL);
lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
lv_obj_set_style_pad_all(scr, 6, 0);
lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
LV_FLEX_ALIGN_CENTER);
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
lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
LV_FLEX_ALIGN_START);
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
lv_obj_set_height(row, 36); /* 关键：固定高度 */
lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE); /* 行自身不可滚 */
lv_obj_set_style_pad_hor(row, 8, 0);
lv_obj_set_style_pad_ver(row, 6, 0);
lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
LV_FLEX_ALIGN_CENTER);
lv_obj_t *icon = lv_label_create(row);
lv_label_set_text(icon, items[i].icon);
lv_obj_add_style(icon, &st_symbol, 0); /* 符号用内置字体 */
lv_obj_t *txt = lv_label_create(row);
lv_label_set_text(txt, items[i].txt);
lv_obj_add_style(txt, &st_body, 0); /* 中文用 FreeType 字体
*/
lv_label_set_long_mode(txt, LV_LABEL_LONG_CLIP); /* 不换行，防止测量重入
*/
lv_obj_set_width(txt, lv_pct(100)); /* 占满剩余宽度 */
}
add_navbar(scr);
return scr;
}
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
g_font_zh = lv_freetype_font_create(path,
LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 14, LV_FREETYPE_FONT_STYLE_NORMAL);
}
#endif
lv_style_init(&st_body);
if (g_font_zh)
lv_style_set_text_font(&st_body, g_font_zh);
lv_style_init(&st_symbol);
lv_style_set_text_font(&st_symbol, LV_FONT_DEFAULT); /* 内置默认字体，含符号
*/
}
/* 
 * 团队模板文件：ui_app.c
 * 已注释所有重复接口，避免与ui_app.cpp冲突
 */

// 注释所有函数实现，仅保留空声明（避免重复定义）
void ui_init(void);

// 以下是原代码的完整注释（确保无语法错误）
/*
// 原ui_init函数（已注释，避免与ui_app.cpp重复）
void ui_init(void)
{
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(240, 320);
    (void)disp;
    lv_keyboard_create();
    init_fonts(); // 先准备字体样式，再创建界面，逐个给到需要的对象
    screens[0] = create_main_screen(0);
    screens[1] = create_list_screen(1);
    screens[2] = create_controls_screen(2);
    current_screen_idx = 0;
    // lv_screen_load(screens[current_screen_idx]);#为方便整合而注释掉/

    // while(1)循环（已注释）
    /*
    while (1)
    {
        uint32_t ms = lv_timer_handler();
        usleep(ms * 1000);
    }
    
}*/

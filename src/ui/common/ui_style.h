#ifndef UI_STYLE_H
#define UI_STYLE_H

#include <lvgl.h>
#include <cstdint>

#define SCREEN_W 240
#define SCREEN_H 320
#define CAM_W 240
#define CAM_H 180

// 颜色定义 (参考原代码中的深色商务风格)
#define THEME_COLOR_PRIMARY      lv_palette_main(LV_PALETTE_BLUE)
#define THEME_COLOR_TEXT_MAIN    lv_color_white()
#define THEME_COLOR_PANEL        lv_color_hex(0x172A45) // 稍亮的蓝灰 (列表/弹窗背景)
#define THEME_COLOR_BAR          lv_color_hex(0x333333) // 顶部栏背景
#define THEME_BG_COLOR           lv_color_hex(0x0F1C2E) // 深邃午夜蓝 (全局背景)
#define THEME_GUTTER             5

// ================= [样式声明] =================
// 字体
LV_FONT_DECLARE(font_noto_16);
extern const lv_font_t* g_font_icon_16; 

// 通用样式对象
extern lv_style_t style_focus_red;    // 红底黄框 (强焦点)
extern lv_style_t style_text_cn;      // 中文字体
extern lv_style_t style_btn_default;  // 默认按钮
extern lv_style_t style_btn_focused;  // 默认按钮聚焦
extern lv_style_t style_base;         // 基础全屏样式
extern lv_style_t style_panel_transp; // 透明面板

/**
 * @brief 初始化所有样式和全局主题
 */
void ui_style_init();

#endif // UI_STYLE_H
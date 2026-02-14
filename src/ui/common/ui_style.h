#ifndef UI_STYLE_H
#define UI_STYLE_H

#include <lvgl.h>
#include <cstdint>

#define SCREEN_W 240
#define SCREEN_H 320
#define CAM_W 240
#define CAM_H 260

#define UI_HEADER_H 30   // 顶部标题栏高度
#define UI_FOOTER_H 30   // 底部状态栏高度

// 颜色定义 (参考原代码中的深色商务风格)
#define THEME_COLOR_PRIMARY      lv_palette_main(LV_PALETTE_BLUE)// 主要蓝色 (按钮/高亮)
#define THEME_COLOR_TEXT_MAIN    lv_color_white()// 主要文本白色
#define THEME_COLOR_PANEL        lv_color_hex(0x172A45) // 稍亮的蓝灰 (列表/弹窗背景)
#define THEME_BG_COLOR           lv_color_hex(0x0055FF) // 深蓝背景 (全局背景)
#define THEME_BAR_GRAD_START     lv_color_hex(0x66CCFF) // 栏位渐变起始（亮蓝）
#define THEME_BAR_GRAD_END       lv_color_hex(0x003399) // 栏位渐变结束（深蓝）
#define THEME_BAR_BORDER         lv_color_hex(0x2195F6) // 栏位边框色
#define THEME_BAR_BG_OPA         LV_OPA_TRANSP // 栏位背景透明度
#define THEME_CONTENT_BG_OPA     LV_OPA_TRANSP// 内容区背景透明度
#define THEME_GUTTER             5

// ================= [样式声明] =================
// 字体
LV_FONT_DECLARE(font_noto_16);
extern const lv_font_t* g_font_icon_16; 

// 通用样式对象
extern lv_style_t style_btn_focused;  // 默认按钮聚焦样式
extern lv_style_t style_text_cn;      // 中文字体
extern lv_style_t style_base;         // 基础全屏样式
extern lv_style_t style_bar_glass;    // 顶部/底部玻璃质感栏
extern lv_style_t style_panel_transp; // 透明面板
extern lv_style_t style_btn_default;  // 默认按钮
extern lv_style_t style_focus_red;    // 红底黄框 (强焦点)



/**
 * @brief 初始化所有样式和全局主题
 */
void ui_style_init();

#endif // UI_STYLE_H
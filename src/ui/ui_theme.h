/**
 * @file ui_theme.h
 * @brief UI 主题配置文件 - 定义颜色、字体、尺寸和通用样式
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

// ================= 1. 颜色定义 (Color Palette) =================
// 使用宏定义，方便一键换肤
#define THEME_COLOR_BG           lv_color_hex(0x000000) // 全局背景黑
#define THEME_COLOR_PANEL        lv_color_hex(0x222222) // 深灰面板/容器
#define THEME_COLOR_BAR          lv_color_hex(0x333333) // 顶部/底部栏
#define THEME_COLOR_BTN_NORMAL   lv_color_hex(0x444444) // 按钮默认色

#define THEME_COLOR_TEXT_MAIN    lv_color_hex(0xFFFFFF) // 主文字白
#define THEME_COLOR_TEXT_HINT    lv_palette_main(LV_PALETTE_GREY) // 提示文字
#define THEME_COLOR_PRIMARY      lv_palette_main(LV_PALETTE_BLUE) // 主题色(蓝)
#define THEME_COLOR_DANGER       lv_palette_main(LV_PALETTE_RED)  // 警告/错误
#define THEME_COLOR_SUCCESS      lv_palette_main(LV_PALETTE_GREEN)// 成功
#define THEME_COLOR_FOCUS        lv_palette_main(LV_PALETTE_YELLOW)// 焦点高亮色

// ================= 2. 尺寸定义 (Dimensions) =================
#define THEME_TOP_BAR_HEIGHT     30
#define THEME_BOTTOM_BAR_HEIGHT  30  // 建议统一高度，原代码中有的地方是110
#define THEME_GUTTER             10  // 通用间距

// ================= 3. 字体声明 (Fonts) =================
// 声明在 ui_theme.cpp 中初始化的字体引用
LV_FONT_DECLARE(font_noto_16); 
#define THEME_FONT_MAIN          &font_noto_16

// ================= 4. 全局样式对象 (Global Styles) =================
// 声明为 extern，让 ui_app.cpp 可以直接使用
extern lv_style_t style_base;           // 基础容器样式
extern lv_style_t style_text_cn;        // 中文文字样式
extern lv_style_t style_btn_default;    // 默认按钮样式
extern lv_style_t style_btn_focused;    // 焦点高亮样式
extern lv_style_t style_panel_transp;   // 透明面板样式 (用于 Grid 容器)

// ================= 5. 接口声明 =================
/**
 * @brief 初始化 UI 主题 (在 lv_init 之后调用)
 */
void ui_theme_init(void);

#endif // UI_THEME_H
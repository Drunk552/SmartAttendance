/**
 * @file ui_theme.cpp
 * @brief UI 主题实现
 */
#include "ui_theme.h"

// 定义全局样式对象
lv_style_t style_base;
lv_style_t style_text_cn;
lv_style_t style_btn_default;
lv_style_t style_btn_focused;
lv_style_t style_panel_transp;

void ui_theme_init(void) {
    // 1. 基础背景样式
    lv_style_init(&style_base);
    lv_style_set_bg_color(&style_base, THEME_COLOR_BG);
    lv_style_set_bg_opa(&style_base, LV_OPA_COVER);
    lv_style_set_text_color(&style_base, THEME_COLOR_TEXT_MAIN);
    lv_style_set_text_font(&style_base, THEME_FONT_MAIN); // 默认应用中文字体

    // 2. 中文文本专用 (其实 style_base 已经涵盖，但保留专用样式更灵活)
    lv_style_init(&style_text_cn);
    lv_style_set_text_font(&style_text_cn, THEME_FONT_MAIN);
    lv_style_set_text_color(&style_text_cn, THEME_COLOR_TEXT_MAIN);

    // 3. 透明容器样式 (用于 Grid 布局的外框)
    lv_style_init(&style_panel_transp);
    lv_style_set_bg_opa(&style_panel_transp, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_panel_transp, 0);
    lv_style_set_pad_all(&style_panel_transp, 0);

    // 4. 默认按钮样式
    lv_style_init(&style_btn_default);
    lv_style_set_bg_color(&style_btn_default, THEME_COLOR_BTN_NORMAL);
    lv_style_set_bg_opa(&style_btn_default, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_default, 8);
    lv_style_set_border_width(&style_btn_default, 0);
    // 布局属性
    lv_style_set_layout(&style_btn_default, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_btn_default, LV_FLEX_FLOW_COLUMN);

    // 5. 焦点/选中样式 (红底黄框)
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, THEME_COLOR_DANGER); // 红底
    lv_style_set_border_width(&style_btn_focused, 3);
    lv_style_set_border_color(&style_btn_focused, THEME_COLOR_FOCUS); // 黄框
    lv_style_set_text_color(&style_btn_focused, THEME_COLOR_FOCUS);   // 黄字
}
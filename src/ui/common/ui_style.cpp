#include "ui_style.h"

// 字体指针初始化
const lv_font_t* g_font_icon_16 = &lv_font_montserrat_16;

// 样式实例
lv_style_t style_focus_red;
lv_style_t style_text_cn;
lv_style_t style_btn_default;
lv_style_t style_btn_focused;
lv_style_t style_base;
lv_style_t style_panel_transp;

// 样式初始化标志
static bool style_inited = false;

// ================= 样式初始化实现 =================
void ui_style_init() {
    if (style_inited) return;

    // 1. 强视觉反馈样式 (红底黄框)
    lv_style_init(&style_focus_red);
    lv_style_set_bg_color(&style_focus_red, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(&style_focus_red, LV_OPA_COVER);
    lv_style_set_border_color(&style_focus_red, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_border_width(&style_focus_red, 3);
    lv_style_set_text_color(&style_focus_red, lv_color_white());

    // 2. 中文字体样式
    lv_style_init(&style_text_cn);
    lv_style_set_text_font(&style_text_cn, &font_noto_16);

    // 3. 基础全屏样式
    lv_style_init(&style_base);
    lv_style_set_bg_color(&style_base, THEME_BG_COLOR);
    lv_style_set_bg_opa(&style_base, LV_OPA_COVER);
    lv_style_set_text_color(&style_base, lv_color_white());

    // 4. 透明面板
    lv_style_init(&style_panel_transp);
    lv_style_set_bg_opa(&style_panel_transp, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_panel_transp, 0);

    // 5. 默认按钮样式
    lv_style_init(&style_btn_default);
    lv_style_set_radius(&style_btn_default, 5);
    lv_style_set_bg_color(&style_btn_default, lv_color_hex(0x444444));
    
    // 6. 默认按钮聚焦样式
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_border_color(&style_btn_focused, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_border_width(&style_btn_focused, 2);

    style_inited = true;
}
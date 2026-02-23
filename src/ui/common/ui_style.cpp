#include "ui_style.h"

// 字体指针初始化
const lv_font_t* g_font_icon_16 = &lv_font_montserrat_16;

// 样式实例
lv_style_t style_btn_focused;  // 默认按钮聚焦样式
lv_style_t style_text_cn;      // 中文字体
lv_style_t style_base;         // 基础全屏样式
lv_style_t style_bar_glass;    // 顶部/底部玻璃质感栏
lv_style_t style_panel_transp; // 透明面板
lv_style_t style_btn_default;  // 默认按钮
lv_style_t style_focus_red;    // 特殊聚焦样式

// ================= 样式初始化实现 =================
void ui_style_init() {
    static bool style_inited = false;// 样式初始化标志
    if (style_inited) return;

    // 1. 默认按钮聚焦样式
    lv_style_init(&style_btn_focused);// 以默认按钮为基础，修改背景色和边框色
    lv_style_set_bg_color(&style_btn_focused, lv_color_hex(0xD2691E)); // 巧克力色背景
    lv_style_set_bg_opa(&style_btn_focused, LV_OPA_COVER); // 不透明
    lv_style_set_border_width(&style_btn_focused, 2);// 加宽边框
    lv_style_set_border_color(&style_btn_focused, lv_color_hex(0xFFCC99)); // 浅杏色边框
    lv_style_set_border_opa(&style_btn_focused, 150); // 边框半透明
    lv_style_set_shadow_width(&style_btn_focused, 10);// 添加阴影效果
    lv_style_set_shadow_color(&style_btn_focused, lv_color_hex(0x8B4513)); // 深棕色阴影
    lv_style_set_shadow_opa(&style_btn_focused, 80);// 阴影半透明
    lv_style_set_text_color(&style_btn_focused, lv_color_white());// 白字

    // 2. 中文字体样式
    lv_style_init(&style_text_cn);
    lv_style_set_text_font(&style_text_cn, &font_noto_16);

    // 3. 基础全屏样式
    lv_style_init(&style_base);// 深蓝背景，白字，去掉默认内边距
    lv_style_set_bg_color(&style_base, THEME_BG_COLOR);// 深蓝背景
    lv_style_set_bg_opa(&style_base, 255);              // 不透明
    lv_style_set_bg_opa(&style_base, LV_OPA_COVER);
    lv_style_set_text_color(&style_base, lv_color_white());

    // 4. 玻璃质感栏位样式 (用于顶部 The Main Menu 和底部操作条)
    lv_style_init(&style_bar_glass);
    lv_style_set_radius(&style_bar_glass, 0); // 直角
    lv_style_set_bg_opa(&style_bar_glass, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bar_glass, THEME_BAR_GRAD_START);
    lv_style_set_bg_grad_color(&style_bar_glass, THEME_BAR_GRAD_END);
    lv_style_set_bg_grad_dir(&style_bar_glass, LV_GRAD_DIR_VER); // 垂直渐变
    lv_style_set_border_width(&style_bar_glass, 2);// 细边框
    lv_style_set_border_color(&style_bar_glass, THEME_BAR_BORDER);
    lv_style_set_border_side(&style_bar_glass, LV_BORDER_SIDE_FULL);

    // 5. 透明面板
    lv_style_init(&style_panel_transp);
    lv_style_set_bg_opa(&style_panel_transp, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_panel_transp, 0);

    // 6. 默认按钮样式
    lv_style_init(&style_btn_default);// 白底半透明，圆角，细边框，白字
    lv_style_set_bg_color(&style_btn_default, lv_color_white()); //白底
    lv_style_set_bg_opa(&style_btn_default, 50); // 半透明效果
    lv_style_set_border_width(&style_btn_default, 1);// 细边框
    lv_style_set_border_color(&style_btn_default, lv_color_white());// 同色边框增加层次感
    lv_style_set_border_opa(&style_btn_default, 80); // 边框也半透明
    lv_style_set_radius(&style_btn_default, 10);// 圆角
    lv_style_set_text_color(&style_btn_default, lv_color_white());// 白字

    // 7.特殊聚焦样式 (正红色背景 + 纯白色的粗边框。利用红白的极致反差，产生类似“紧急制动”按钮的刺眼效果。)，用于重要操作的二次确认界面
    lv_style_init(&style_focus_red);
    lv_style_set_bg_color(&style_focus_red, lv_palette_main(LV_PALETTE_RED)); // 纯正大红色
    lv_style_set_bg_opa(&style_focus_red, LV_OPA_COVER);
    lv_style_set_border_width(&style_focus_red, 4); // 粗边框
    lv_style_set_border_color(&style_focus_red, lv_color_white()); // 纯白边框，拉高对比度
    lv_style_set_radius(&style_focus_red, 8); // 圆角稍微变大一点

    style_inited = true;
}
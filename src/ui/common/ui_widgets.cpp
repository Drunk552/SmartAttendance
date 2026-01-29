#include "ui_widgets.h"
#include "ui_style.h"
#include <cstring>
#include <cstdio>

// ================= 实现标准系统菜单按钮 (Grid Item) =================
lv_obj_t* create_sys_grid_btn(
    lv_obj_t *parent, int row,
    const char* icon, const char* text_en, const char* text_cn,
    lv_event_cb_t event_cb, const char* user_data
) {
    lv_obj_t *btn = lv_button_create(parent);
    
    // 设置 Grid 位置 (占1列)
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, row, 1);

    // 应用样式
    lv_obj_add_style(btn, &style_btn_default, 0);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);

    // 布局: 横向排列
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 10, 0); 
    lv_obj_set_style_pad_gap(btn, 10, 0); 

    // 绑定事件
    // 注意：const char* user_data 需要确保生命周期，通常传字符串字面量是安全的
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, (void*)user_data);

    // 1. 图标
    lv_obj_t *lbl_icon = lv_label_create(btn);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_style_text_font(lbl_icon, g_font_icon_16, 0);

    // 2. 文字
    lv_obj_t *lbl_text = lv_label_create(btn);
    if (text_en && strlen(text_en) > 0) {
        lv_label_set_text_fmt(lbl_text, "%s  %s", text_en, text_cn);
    } else {
        lv_label_set_text(lbl_text, text_cn);
    }
    lv_obj_add_style(lbl_text, &style_text_cn, 0);

    return btn;
}

// ================= 实现通用弹窗和关闭回调 =================
void mbox_close_event_cb(lv_event_t * e) {
    lv_obj_t * mbox = (lv_obj_t *)lv_event_get_user_data(e);
    if(mbox) lv_msgbox_close(mbox);
}

// 显示通用弹窗
void show_popup(const char* title, const char* msg) {
    lv_obj_t* mbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, msg);
    lv_obj_t* btn = lv_msgbox_add_footer_button(mbox, "Close");
    lv_obj_add_event_cb(btn, mbox_close_event_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_center(mbox);
}
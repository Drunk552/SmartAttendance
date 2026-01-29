#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <lvgl.h>

/**
 * @brief 创建标准系统菜单按钮 (Grid Item) - 红底黄框风格
 * @param parent 父对象 (Grid)
 * @param row Grid 行索引
 * @param icon 图标符号
 * @param text_en 英文文本
 * @param text_cn 中文文本
 * @param event_cb 事件回调
 * @param user_data 用户数据
 */
lv_obj_t* create_sys_grid_btn(
    lv_obj_t *parent, int row,
    const char* icon, const char* text_en, const char* text_cn,
    lv_event_cb_t event_cb, const char* user_data
);

/**
 * @brief 显示通用弹窗
 */
void show_popup(const char* title, const char* msg);

/**
 * @brief 通用 MsgBox 关闭回调
 */
void mbox_close_event_cb(lv_event_t * e);

#endif // UI_WIDGETS_H
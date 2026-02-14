#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <lvgl.h>

struct BaseScreenParts {
    lv_obj_t* screen;   // 整个屏幕对象 (注册给UiManager用)
    lv_obj_t* content;  // 中间空白区域 (放具体业务控件用)
    lv_obj_t* header;   // 头部对象 (如果需要动态改标题)
    lv_obj_t* footer;   // 底部对象 (如果需要改提示语)
    lv_obj_t * lbl_title;// 标题标签 (如果需要动态改标题)
    lv_obj_t * lbl_time;// 时间标签 (如果需要动态更新时间)
};

/**
 * @brief 创建标准基础页面 (Header + Content + Footer)
 * @param title 页面标题文字 (如 "系统设置")
 * @return BaseScreenParts 包含屏幕指针和内容区指针
 */
BaseScreenParts create_base_screen(const char* title);

/**
 * @brief 设置底部提示语
 */
void set_base_footer_hint(lv_obj_t* footer, const char* left_text, const char* right_text = nullptr);

/**
 * @brief 创建标准系统菜单按钮 (Grid Item)
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
 * @brief 创建一个标准的居中面板（用于表单、弹窗等）
 * 特性：
 * 1. 自动居中 (上下左右)
 * 2. 高度自适应内容 (LV_SIZE_CONTENT)
 * 3. 隐藏滚动条
 * 4. 内部采用垂直 Flex 布局
 * 5. 带半透明背景样式
 * * @param parent 父对象 (通常是 parts.content)
 * @param width 面板宽度 (默认 230，适配 240 宽度的屏幕)
 * @return lv_obj_t* 创建好的容器指针
 */
lv_obj_t* create_center_panel(lv_obj_t* parent, int width = 230);

/**
 * @brief 创建标准菜单网格容器 (用于九宫格/列表菜单)
 * 特性：
 * 1. 自动在父对象中居中
 * 2. 隐藏滚动条
 * 3. 预设网格布局 (默认2列，适应240宽屏幕)
 * @param parent 父对象
 * @return lv_obj_t* 容器对象
 */
lv_obj_t* create_menu_grid_container(lv_obj_t* parent);

/**
 * @brief 显示通用弹窗
 */
void show_popup(const char* title, const char* msg);

/**
 * @brief 通用 MsgBox 关闭回调
 */
void mbox_close_event_cb(lv_event_t * e);

#endif // UI_WIDGETS_H
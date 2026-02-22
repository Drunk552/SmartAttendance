#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <lvgl.h>

#include <vector>
#include <string>
#include <utility> // std::pair 需要用到这个

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
 * @brief 创建标准系统列表按钮 (专为 Flex 列表设计，宽度自动 100%)
 * @param parent 父对象 (Flex 列表容器)
 * @param icon 图标符号
 * @param text_en 英文文本
 * @param text_cn 中文文本
 * @param event_cb 事件回调
 * @param user_data 用户数据
 */
lv_obj_t* create_sys_list_btn(
    lv_obj_t *parent,
    const char* icon, const char* text_en, const char* text_cn,
    lv_event_cb_t event_cb, const char* user_data
);

/**
 * @brief 创建标准垂直列表容器 (支持自动滚动、顶部对齐，使用 Flex 布局)
 * @param parent 父对象 (通常是 parts.content)
 */
lv_obj_t* create_list_container(lv_obj_t* parent);

/**
 * @brief 创建标准的表单输入组 (Label + TextArea)
 * * @param parent           父容器 (通常是 form_cont)
 * @param label_text       左侧/上方标题 (如 "原始姓名:")
 * @param placeholder_text 提示文字 (如 "请输入姓名"，如果传 nullptr 则不显示)
 * @param initial_text     初始填充的文字 (如数据库读出来的名字，如果传 nullptr 则为空)
 * @param is_readonly      是否为只读模式 (true 会变灰且不可点击)
 * @return lv_obj_t* 返回创建好的文本输入框指针 (方便后续绑定事件或读取文本)
 */
lv_obj_t* create_form_input(
    lv_obj_t *parent, 
    const char *label_text, 
    const char *placeholder_text, 
    const char *initial_text, 
    bool is_readonly
);

/**
 * @brief 创建一个通用的表单下拉框
 * @param parent    父容器
 * @param title     左侧标题 (如 "新部门:", "新班次:")
 * @param items     选项列表，格式为 vector<pair<数据的ID, 数据的名称>>
 * @param default_id 默认选中的数据 ID
 * @return lv_obj_t* 返回下拉框对象本体
 */
lv_obj_t* create_form_dropdown(
    lv_obj_t* parent,
    const char* title,
    const std::vector<std::pair<int,
    std::string>>& items,
    int default_id
);

/**
 * @brief 创建标准表单容器 (支持自动滚动、垂直Flex布局、带有统一的表单间距)
 * @param parent 父对象 (通常是 parts.content)
 */
lv_obj_t* create_form_container(lv_obj_t* parent);

/**
 * @brief 创建标准表单按钮
 * @param parent     父容器 (通常是 form_cont)
 * @param btn_text   按钮上的文字 (如 "确认修改", "保存")
 * @param event_cb   按钮点击/按下的回调函数
 * @param user_data  传递给回调函数的用户数据 (比如你需要传入的 ta_new)
 * @return lv_obj_t* 返回创建好的按钮指针
 */
lv_obj_t* create_form_btn(lv_obj_t *parent, const char *btn_text, lv_event_cb_t event_cb, void *user_data);

// 异步销毁任务 
static void popup_close_async_task(void * p);

//  弹窗销毁的回调函数 
static void popup_close_event_cb(lv_event_t * e);

/**
 * @brief 显示一个通用单按钮弹窗
 * @param title 弹窗标题 (传 nullptr 则不显示标题)
 * @param msg   弹窗内容
 * @param focus_back_obj 弹窗关闭后，焦点需要回到哪个控件上 (防止焦点丢失)
 */
void show_popup_msg(const char* title, const char* msg, lv_obj_t* focus_back_obj = nullptr, const char* btn_text = "确认");

/**
 * @brief 显示通用弹窗
 */
void show_popup(const char* title, const char* msg);

/**
 * @brief 通用 MsgBox 关闭回调
 */
void mbox_close_event_cb(lv_event_t * e);

#endif // UI_WIDGETS_H
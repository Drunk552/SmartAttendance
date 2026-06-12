#ifndef HAL_KEYPAD_H
#define HAL_KEYPAD_H

#include <lvgl.h>

/**
 * @brief 矩阵键盘输入模式
 * - NUMERIC : 数字模式，按 1~9/0 直接输出数字
 * - ENGLISH : 英文模式，T9 连按切换字母，超时 1s 自动提交
 * - CHINESE : 中文模式，T9 组合拼音，候选字条显示汉字，OK 确认插入
 */
enum class HalInputMode {
    NUMERIC = 0,
    ENGLISH = 1,
    CHINESE = 2,
};

/** 获取当前输入模式 */
HalInputMode hal_keypad_get_mode();

/** 设置输入模式（可外部强制设定） */
void hal_keypad_set_mode(HalInputMode mode);

/** 获取当前模式的显示字符串，例如 "123"、"ABC"、"拼" */
const char* hal_keypad_get_mode_str();

/** 设置模式切换后的通知回调（使用者在 UI 层添加，用于刷新模式指示标签） */
void hal_keypad_set_mode_change_cb(void (*cb)(HalInputMode new_mode));

/**
 * @brief 注册当前屏幕的中文候选字条控件
 *        由各个屏幕初始化时调用（BaseScreenParts.candidate_bar）
 *        候选字条显隐和内容刷新均由 hal_keypad 内部在按键处理时调用
 * @param bar lv_obj_t* 候选字条对象（create_base_screen 返回的 candidate_bar 字段）
 *            传 nullptr 表示当前屏幕无候选字条（注销）
 */
void hal_keypad_set_candidate_bar(lv_obj_t* bar);

/**
 * @brief 注册当前聚焦的 TextArea，中文模式 OK 确认时向其插入所选汉字
 * @param ta lv_obj_t* 目标 textarea，传 nullptr 表示注销
 */
void hal_keypad_set_target_textarea(lv_obj_t* ta);

/** 初始化物理键盘，并注册到 LVGL */
lv_indev_t * hal_keypad_init(void);

#endif // HAL_KEYPAD_H
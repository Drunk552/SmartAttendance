#ifndef UI_USER_MGMT_H
#define UI_USER_MGMT_H

#include <lvgl.h>

namespace ui {
namespace user_mgmt {

/**
 * @brief 初始化员工管理模块内部状态
 */
void user_init();

/**
 * @brief 加载员工管理主菜单 (Grid 菜单)
 */
void load_user_menu_screen();

/**
 * @brief 加载员工列表页
 */
void load_user_list_screen();

/**
 * @brief 启动注册向导 (Step 1: 填写表单)
 */
void load_user_register_form();

/**
 * @brief 加载注册向导 (Step 2: 拍照)
 * 通常由 Step 1 的"下一步"按钮触发
 */
void load_user_register_camera_step();

/**
 * @brief 加载员工详情页
 * @param user_id 员工ID
 */
void load_user_info_screen(int user_id);

/**
 * @brief 修改姓名界面
 */
void load_user_edit_name_screen();

/**
 * @brief 修改部门界面
 */
void load_user_edit_dept_screen();

/**
 * @brief 注册密码界面
 */
void load_user_register_password_screen();

/**
 * @brief 修改密码界面
 */
void load_user_edit_password_screen();

/**
 * @brief 加载删除员工确认页
 */
void load_user_delete_screen();

/**
 * @brief 加载权限变更页 (Level 3-B)
 * @param user_id 员工ID
 * @param current_role 当前权限
 */
void load_user_role_change_screen(int user_id, int current_role);

} // namespace user_mgmt
} // namespace ui

#endif // UI_USER_MGMT_H
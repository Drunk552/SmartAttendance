/**
 * @file ui_app.cpp
 * @brief UI 层 - v2.1 (Focus Debug Mode) - C++ 风格改写
 * @details 将文件从 C 风格迁移为更符合 C++ 的写法：
 *  - 使用 std::string / std::array 等
 *  - 使用 nullptr 替换 NULL
 *  - 使用 snprintf / std::string 替换 sprintf / char buffer 处理
 *  - 其它小幅现代化改进（保持 LVGL 回调接口不变）
 */
#include "ui_app.h"
#include <lvgl.h>
#include <unistd.h>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <string>
#include <array>
#include <set>
#include <algorithm>
#include <vector>
#include <ctime>
#include <sstream>
#include "lv_conf.h"
#include <unistd.h> // for sleep (模拟耗时)
#include <filesystem> // C++17 标准文件系统库
#include "ui_controller.h"
#include "ui_theme.h"
#include "../business/event_bus.h"

// 声明你的自定义字体
LV_FONT_DECLARE(font_noto_16);

// 声明 LVGL 内置字体 (用于显示图标)
// 如果你的 lv_conf.h 没开启 16 号字体，这里可以改成 lv_font_montserrat_14
LV_FONT_DECLARE(lv_font_montserrat_16);

// ================= 宏定义 =================
#define SCREEN_W 240
#define SCREEN_H 320
#define CAM_W 240
#define CAM_H 180

// ================= 全局变量 =================
static lv_obj_t *screen_main = nullptr; 
static lv_obj_t *screen_menu = nullptr; 
static lv_obj_t *obj_grid = nullptr; 
static lv_group_t *g_keypad_group = nullptr; 
// [Epic 3.3 新增]
static lv_obj_t *screen_list = nullptr;    // 列表页屏幕
static lv_obj_t *obj_list_view = nullptr;  // 列表控件容器
// [Epic 3.3 注册向导] 全局变量
static lv_obj_t *screen_register = nullptr;
static lv_obj_t *ta_name = nullptr;      // 名字输入框
static lv_obj_t *img_face_reg = nullptr; // 注册页面的摄像头预览
static lv_obj_t *screen_user_info = nullptr;    // Level 2: 详情页
static lv_obj_t *screen_pwd_change = nullptr;   // Level 3-A: 改密页
static lv_obj_t *screen_role_auth = nullptr;    // Level 3-B: 权限变更页
// [Epic 3.4 新增] 考勤记录页
static lv_obj_t *screen_records = nullptr;
static lv_obj_t *obj_record_list = nullptr;
// [Epic 4.3 新增] 磁盘空间警告图标
static lv_obj_t *label_disk_warn = nullptr; // 磁盘警告图标
static bool g_disk_full = false;            // 磁盘满标志
// [Epic 4.3] 维护菜单相关
static lv_obj_t *screen_sys_ops = nullptr;
static lv_obj_t *obj_sys_list = nullptr;
static lv_obj_t *screen_user_mgmt = nullptr;   // 新增：员工管理屏幕
static lv_obj_t *obj_user_mgmt_grid = nullptr; // 新增：员工管理菜单容器
// [Epic 5.2 新增] 记录查询相关变量
static lv_obj_t *screen_rec_query = nullptr;   // 查询输入页
static lv_obj_t *ta_query_id = nullptr;        // ID输入框
static lv_obj_t *screen_rec_result = nullptr;  // 结果展示页
static lv_obj_t *obj_result_container = nullptr; // 结果页容器
static lv_obj_t *screen_sys_settings = nullptr; // 一级菜单
static lv_obj_t *obj_sys_grid = nullptr;        // 一级菜单的网格容器 (用于修复焦点问题)
static lv_obj_t *screen_sys_adv = nullptr;      // 二级菜单 (高级设置)
static lv_obj_t *obj_adv_grid = nullptr;        //  二级菜单的网格容器
// System Info 全局变量
static lv_obj_t *screen_sys_info = nullptr;     // Level 1: 系统信息菜单
static lv_obj_t *obj_info_grid = nullptr;       // 信息菜单容器
static lv_obj_t *screen_storage_info = nullptr; // Level 2: 存储详情页
static lv_obj_t *btn_query_back = nullptr; //  返回按钮指针
static lv_obj_t *btn_result_back = nullptr; //  结果页返回按钮
// 强视觉反馈样式 (红底黄框)
static lv_style_t style_focus_red;
static bool style_focus_red_inited = false;

// 这些变量用于在注册流程中临时存储数据
int g_reg_user_id = 0;      // 全局变量：注册时的工号
std::string g_reg_name = ""; // 全局变量：注册时的姓名
int g_reg_dept_id = 0;      // 全局变量：注册时的部门ID

// ================= 摄像头多线程优化变量 (Epic 4.4) =================
// 1. 显示缓冲区 (UI 线程读取) - 替换原有的 cam_buf
static std::array<uint8_t, CAM_W * CAM_H * 3> cam_buf_display;

extern volatile bool g_program_should_exit; // 引用 main.cpp 中的退出标志

static lv_obj_t *img_camera = nullptr;

static lv_obj_t *label_time = nullptr;

// ================= 核心导航函数前向声明 =================
static void load_main_screen(void);
static void load_menu_screen(void);
static void load_record_screen(void);
static void request_exit(void);
// [Epic 4.3 新增] 
static void load_sys_ops_screen(void);
static void mbox_close_event_cb(lv_event_t * e);
// [Epic 3.3 新增]
static void create_user_list_screen(void);
static void load_user_list_screen(void);
static void list_btn_event_cb(lv_event_t *e);

// [Epic 3.3 注册向导] 函数声明
static void create_register_screen(void);
static void load_register_step(void); // 采集人脸
static void load_user_mgmt_screen(void); // 新增：声明加载员工管理页函数
static void load_record_query_screen(void);
static void load_record_result_screen(int user_id);
// System Settings 前向声明
static void load_sys_settings_screen(void);
static void load_sys_adv_screen(void);
static void load_sys_info_screen(void);
static void load_storage_info_screen(void);
static void load_user_info_screen(int user_id);// 详情页
static void load_password_change_screen(int user_id);// 改密页
static void load_role_change_auth_screen(int user_id, int current_role);// 权限变更页

// 前向声明：确保编译器知道这个函数存在 // 或者你原来返回上一级菜单的函数名
void load_register_form_screen();

// 摄像头图像描述符 (v9 格式)
static lv_image_dsc_t img_dsc = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB888,
        .flags = 0,
        .w = CAM_W,
        .h = CAM_H,
        .stride = CAM_W * 3,
        .reserved_2 = 0
    },
    .data_size = CAM_W * CAM_H * 3,
    .data = nullptr, // 稍后在 create_main_screen 中赋值
    .reserved = 0
};

// ================= 辅助函数 =================

/**
 * @brief 初始化强视觉反馈样式 (红底黄框)
 */
static void init_focus_style() {
    if (style_focus_red_inited) return;
    
    lv_style_init(&style_focus_red);
    
    // 背景：鲜艳红色
    lv_style_set_bg_color(&style_focus_red, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(&style_focus_red, LV_OPA_COVER);
    
    // 边框：黄色，3px
    lv_style_set_border_color(&style_focus_red, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_border_width(&style_focus_red, 3);
    
    // 文本：白色
    lv_style_set_text_color(&style_focus_red, lv_color_white());
    
    style_focus_red_inited = true;
}

// ================= [内存管理核心] =================

/**
 * @brief 安全销毁指定屏幕及其子控件，并置空所有相关全局指针
 * 防止定时器访问悬空指针导致崩溃
 */
static void free_screen_resources(lv_obj_t** screen_ptr) {
    if (screen_ptr == nullptr || *screen_ptr == nullptr) return;

    // 1. 根据屏幕指针，置空该屏幕下的所有全局子控件指针
    // 必须与 create_xxx_screen 中赋值的全局变量一一对应
    if (screen_ptr == &screen_main) {
        label_time = nullptr;
        label_disk_warn = nullptr;
        img_camera = nullptr;
    }
    else if (screen_ptr == &screen_menu) {
        obj_grid = nullptr;
    }
    else if (screen_ptr == &screen_user_mgmt) {
        obj_user_mgmt_grid = nullptr;
    }
    else if (screen_ptr == &screen_list) {
        obj_list_view = nullptr;
    }
    else if (screen_ptr == &screen_register) {
        ta_name = nullptr;
        img_face_reg = nullptr;
        // 注意：g_reg_xxx 是数据变量，不需要置空
    }
    else if (screen_ptr == &screen_rec_query) {
        ta_query_id = nullptr;
        btn_query_back = nullptr;
    }
    else if (screen_ptr == &screen_rec_result) {
        obj_result_container = nullptr;
        btn_result_back = nullptr;
    }
    else if (screen_ptr == &screen_sys_settings) {
        obj_sys_grid = nullptr;
    }
    else if (screen_ptr == &screen_sys_adv) {
        obj_adv_grid = nullptr;
    }
    else if (screen_ptr == &screen_sys_info) {
        obj_info_grid = nullptr;
    }
    // screen_storage_info 没有全局子控件，无需特殊处理

    // 如果是新屏幕，不需要特殊置空子对象（因为我们没用全局变量存子对象），直接通过
    if (screen_ptr == &screen_user_info) {
        // 这里的子对象都是局部的，不需要置空全局变量
    }

    // 2. 销毁 LVGL 对象 (这会递归销毁所有子对象)
    lv_obj_delete(*screen_ptr);
    
    // 3. 将屏幕指针置空，标记为已销毁
    *screen_ptr = nullptr;
    
    std::printf("[Memory] Screen Freed.\n");
}

/**
 * @brief 真正的清理逻辑，由定时器回调执行
 * 此时之前的事件处理已彻底完成，可以安全销毁旧对象
 */
static void async_screen_cleanup_cb(lv_timer_t * t) {
    // 1. 获取当前系统真正正在显示的屏幕
    lv_obj_t * act_scr = lv_screen_active(); // 获取当前活跃屏幕
    lv_obj_t * keep_scr = (lv_obj_t *)lv_timer_get_user_data(t);
    // 2. 维护所有屏幕指针的列表
    lv_obj_t** all_screens[] = {
        &screen_main, 
        &screen_menu, 
        &screen_user_mgmt, 
        &screen_list, 
        &screen_register,
        &screen_rec_query, 
        &screen_rec_result,
        &screen_sys_settings, 
        &screen_sys_adv, 
        &screen_sys_info,
        &screen_storage_info,
        &screen_user_info,      // Level 2
        &screen_pwd_change,     // Level 3-A
        &screen_role_auth       // Level 3-B
    };

    // 3. 遍历销毁所有“非当前显示”的屏幕
    for (auto ptr : all_screens) {
        // 如果屏幕已创建(非空) 且 不是当前活跃屏幕，则销毁
        if (*ptr != nullptr && *ptr != act_scr && *ptr != keep_scr) {
            free_screen_resources(ptr);
        }
    }
}

/**
 * @brief 启动异步销毁任务
 * 原来的参数 active_screen 被忽略，改用 lv_screen_active() 动态判断，防止竞态问题
 */
static void destroy_all_screens_except(lv_obj_t* screen_to_keep) {
    // 创建一个 10ms 的单次定时器
    // 这样可以确保当前的按键事件回调完全执行完毕后，再执行销毁
    lv_timer_t * t = lv_timer_create(async_screen_cleanup_cb, 10, screen_to_keep);
    lv_timer_set_repeat_count(t, 1); // 只运行一次，运行完自动删除定时器
}

static void request_exit(void) {
    std::printf("[UI] Requesting Exit...\n");
    g_program_should_exit = true; 
}

// ================= [Epic 4.2] 报表导出逻辑 =================

static void ui_download_report_handler() {
    // 1. 弹出提示 "正在检测 U 盘..."
    lv_obj_t * mbox = nullptr;

    lv_obj_center(mbox);
    lv_timer_handler(); // 强制刷新 UI 显示提示框
    
    bool success = UiController::getInstance()->exportReportToUsb();// 调用封装后的报表导出接口

    // 2. 关闭 "检测中" 的提示框
    lv_obj_delete(mbox); // v9 使用 lv_obj_delete 删除对象


    // 3. 显示最终结果
    mbox = lv_msgbox_create(NULL);
    if (success) {
        lv_msgbox_add_title(mbox, "Export Success");
        lv_msgbox_add_text(mbox, "Saved to:\n/output/usb_sim/");
    } else {
        lv_msgbox_add_title(mbox, "Export Failed");
        lv_msgbox_add_text(mbox, "Write error or No data.");
    }
    lv_msgbox_add_close_button(mbox); // v9 添加关闭按钮

}

// ================= 事件处理 =================

static void main_screen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) load_menu_screen();
        if (key == LV_KEY_ESC) request_exit();
    }
}

// [升级版] 支持真正的上下左右二维导航
static void menu_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    // 使用 C 风格强转：
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = static_cast<const char*>(lv_event_get_user_data(e));

    // 仅处理按键事件
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 获取当前按钮在 Grid 中的索引 (0, 1, 2, 3)
        // 0:左上, 1:右上, 2:左下, 3:右下
        lv_obj_t *grid = lv_obj_get_parent(btn);
        uint32_t index = lv_obj_get_index(btn); 
        uint32_t total = lv_obj_get_child_cnt(grid);
        int next_index = -1; // 目标索引

        // --- 核心导航逻辑 ---
        if (key == LV_KEY_RIGHT) {
            // 向右：+1，循环
            next_index = (index + 1) % total;
            std::printf("[UI] Nav: RIGHT (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_LEFT) {
            // 向左：-1，循环 (加 total 防止负数)
            next_index = (index + total - 1) % total;
            std::printf("[UI] Nav: LEFT (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_DOWN) {
            // 向下：+2 (因为是2列布局)，循环
            next_index = (index + 2) % total;
            std::printf("[UI] Nav: DOWN (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_UP) {
            // 向上：-2，循环
            next_index = (index + total - 2) % total;
            std::printf("[UI] Nav: UP (%d -> %d)\n", index, next_index);
        }

        // --- 执行跳转 ---
        if (next_index >= 0) {
            // 找到目标按钮
            lv_obj_t *target_btn = lv_obj_get_child(grid, next_index);
            // 强制聚焦
            lv_group_focus_obj(target_btn);
            return; // 完成跳转，直接返回
        }

        // --- 处理功能键 ---
        if (key == LV_KEY_ESC) {
            std::printf("[UI] ESC -> Back\n");
            load_main_screen();
        }
        else if (key == LV_KEY_ENTER) {
            std::printf("[UI] Action: %s\n", tag);

            if(std::strcmp(tag, "UserMgmt") == 0) {
                load_user_mgmt_screen();
            }
            else if(std::strcmp(tag, "Records") == 0) {
                 // 跳转到查询界面
                 load_record_query_screen();
            }
            else if(std::strcmp(tag, "Report") == 0) {
                 // [Epic 4.2] 调用报表导出
                 ui_download_report_handler();
            }
            else if(std::strcmp(tag, "Settings") == 0) {
                if (g_disk_full) {
                    // [Epic 4.3] 磁盘满拦截
                    lv_obj_t * mbox = lv_msgbox_create(NULL);
                    lv_msgbox_add_title(mbox, "Error");
                    lv_msgbox_add_text(mbox, "Disk Full! Cannot register.");
                    lv_obj_t * btn = lv_msgbox_add_footer_button(mbox, "Close");
                    lv_obj_add_event_cb(btn, mbox_close_event_cb, LV_EVENT_CLICKED, mbox);
                    lv_obj_center(mbox);
                } else {
                    load_register_form_screen(); 
                }
            }
            else if(std::strcmp(tag, "System") == 0) {
                // 进入维护菜单，而不是直接退出
                load_sys_ops_screen();
            }
            else if(std::strcmp(tag, "SysInfo") == 0) {
                load_sys_info_screen(); // 跳转到系统信息
            }
        }
    }
    
    // 保留点击支持
    if (code == LV_EVENT_CLICKED) {
         std::printf("[UI] Click: %s\n", tag);
         if(std::strcmp(tag, "System") == 0) {
            extern volatile bool g_program_should_exit;
            g_program_should_exit = true; 
        }  
    }
}

// ================= [System Settings] 业务逻辑接口 =================
static void on_clear_all_records() {
    std::printf("[System] Action: Clear All Records\n");
    // TODO: 调用 db_clear_attendance(); 
    // ui_show_toast("Records Cleared!");
}

static void on_clear_all_employees() {
    std::printf("[System] Action: Clear All Employees\n");
    // TODO: 调用 db_clear_users();
}

static void on_clear_all_data() {
    std::printf("[System] Action: Clear All Data\n");
    // TODO: 清空所有表 + 删除特征文件
}

static void on_factory_reset() {
    std::printf("[System] Action: Factory Reset\n");
    // TODO: 删除配置文件 + 重启
}

// ================= [System Settings] 辅助封装 =================
/**
 * @brief 创建标准系统菜单按钮 (Grid Item) - 红底黄框风格
 */
static lv_obj_t* create_sys_grid_btn(lv_obj_t *parent, int row, 
                                     const char* icon, const char* text_en, const char* text_cn, 
                                     lv_event_cb_t event_cb, const char* user_data) 
{
    lv_obj_t *btn = lv_button_create(parent);
    
    // 设置 Grid 位置
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, row, 1);

    // 应用按钮样式
    lv_obj_add_style(btn, &style_btn_default, 0);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);

    // 布局: 横向排列 (Row)
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    // 关键：左对齐，垂直居中
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 10, 0); // 增加一点内边距
    lv_obj_set_style_pad_gap(btn, 10, 0); // 图标和文字的间距

    // 绑定事件
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, (void*)user_data);

    // 1. 图标
    lv_obj_t *lbl_icon = lv_label_create(btn);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_16, 0);

    // 2. 文字 (合并为一个 Label，格式为 "英文  中文")
    lv_obj_t *lbl_text = lv_label_create(btn);
    // 使用 format 拼接字符串，中间加两个空格
    lv_label_set_text_fmt(lbl_text, "%s  %s", text_en, text_cn);
    
    // 3. 字体样式 (必须应用中文字体全局样式)
    lv_obj_add_style(lbl_text, &style_text_cn, 0);

    return btn;
}

static void create_main_screen(void) {
    screen_main = lv_obj_create(nullptr);
    lv_obj_add_style(screen_main, &style_base, 0);
    lv_obj_set_scrollbar_mode(screen_main, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(screen_main, main_screen_event_cb, LV_EVENT_ALL, nullptr);

    // Top Bar
    lv_obj_t *top = lv_obj_create(screen_main);
    lv_obj_set_size(top, SCREEN_W, 30);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);

    // 样式设置：深灰背景，无边框
    lv_obj_set_style_bg_color(top, lv_color_hex(0x333333), 0); 
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);

    // 1. 时间标签 (居中)
    label_time = lv_label_create(top);
    lv_label_set_text(label_time, "00:00");
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);
    // 强制设置字体颜色为纯亮白 (#FFFFFF)
    // 确保在深灰背景上对比度最大
    lv_obj_set_style_text_color(label_time, THEME_COLOR_TEXT_MAIN, 0);

    // ---------------- [Epic 4.3 新增代码 START] ----------------
    // 2. 磁盘满警告图标 (右上角，默认隐藏)
    // 注意：label_disk_warn 需要在文件顶部定义为全局变量: static lv_obj_t *label_disk_warn = nullptr;
    label_disk_warn = lv_label_create(top);
    
    // 设置文本为 图标+文字 (使用 LVGL 内置符号)
    
    lv_obj_set_style_text_font(label_disk_warn, &lv_font_montserrat_16, 0); 
    
    // 设置颜色为红色，起到警示作用
    lv_obj_set_style_text_color(label_disk_warn, lv_palette_main(LV_PALETTE_RED), 0);
    
    // 对齐到右上角，留出 5px 的右边距
    lv_obj_align(label_disk_warn, LV_ALIGN_RIGHT_MID, -5, 0);
    
    // 关键：默认添加 HIDDEN 标志，只有检测到空间不足时才移除此标志显示出来
    lv_obj_add_flag(label_disk_warn, LV_OBJ_FLAG_HIDDEN);

    // Camera
    img_dsc.header.w = CAM_W;
    img_dsc.header.h = CAM_H;

    // [Epic 4.4 修改] 指向新的显示缓冲区
    img_dsc.data = cam_buf_display.data(); 
    img_dsc.data_size = static_cast<uint32_t>(cam_buf_display.size());
   
        img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
        img_dsc.header.stride = CAM_W * 3;
        img_camera = lv_image_create(screen_main);
        lv_image_set_src(img_camera, &img_dsc);

    lv_obj_set_size(img_camera, CAM_W, CAM_H);
    lv_obj_align(img_camera, LV_ALIGN_TOP_MID, 0, 40);

    // Bottom
    lv_obj_t *bottom = lv_obj_create(screen_main);
    lv_obj_set_size(bottom, SCREEN_W, 110);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom, THEME_COLOR_PANEL, 0);
    
    lv_obj_t *tip = lv_label_create(bottom);
    lv_label_set_text(tip, "Enter: Menu\nESC: Exit"); 
    lv_obj_set_style_text_color(tip, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(tip, LV_ALIGN_CENTER, 0, 0);
}

// 定义菜单项结构体 (放在函数内部或外部均可)
struct MenuEntry {
    const char* icon;       // 图标
    const char* text_en;    // 英文标题
    const char* text_cn;    // 中文标题
    const char* event_tag;  // 事件回调用的 Tag (保持不变以兼容逻辑)
};

static void create_menu_screen(void) {
    // 1. 创建屏幕
    screen_menu = lv_obj_create(nullptr);
    lv_obj_add_style(screen_menu, &style_base, 0);
    
    // 2. 标题 (System Menu / 系统菜单)
    lv_obj_t *title = lv_label_create(screen_menu);
    lv_label_set_text(title, "System Menu / 系统菜单");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    // 使用全局样式 style_text_cn应用中文字体
    lv_obj_add_style(title, &style_text_cn, 0); 

    // 4. 定义九宫格布局
    static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; 

    obj_grid = lv_obj_create(screen_menu); 
    lv_obj_set_size(obj_grid, LV_PCT(90), LV_PCT(80));
    lv_obj_align(obj_grid, LV_ALIGN_BOTTOM_MID, 0, -10); // 靠下居中
    lv_obj_set_layout(obj_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_grid, col_dsc, row_dsc);
    
    lv_obj_add_style(obj_grid, &style_panel_transp, 0);// 使用透明样式
    lv_obj_set_style_bg_opa(obj_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_grid, 0, 0);
    lv_obj_set_style_pad_column(obj_grid, THEME_GUTTER, 0);
    lv_obj_set_style_pad_row(obj_grid, THEME_GUTTER, 0);

    // 5. 菜单内容定义
    struct MenuEntry {
        const char* icon;
        const char* text_en;
        const char* text_cn;
        const char* event_tag;
    };

    MenuEntry menu_items[] = {
    {LV_SYMBOL_DIRECTORY,"User Mgmt", "员工管理", "UserMgmt"}, // 新增入口
    {LV_SYMBOL_EYE_OPEN, "Records",   "记录查询", "Records"},
    {LV_SYMBOL_DRIVE,    "Report",    "导出报表", "Report"},
    {LV_SYMBOL_SETTINGS, "System",    "系统设置", "System"},// 原Settings和System合并或保留其一
    {LV_SYMBOL_LIST,     "Info",      "系统信息", "SysInfo"}    
    };
    
    // 6. 循环创建按钮
    for(int i = 0; i < 5; i++) {
        uint8_t col = i % 2;
        uint8_t row = i / 2;

        // 创建按钮主体
        lv_obj_t *btn = lv_button_create(obj_grid);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                                  LV_GRID_ALIGN_STRETCH, row, 1);
        
        lv_obj_add_style(btn, &style_btn_default, 0); 
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);

        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(btn, 2, 0); 

        // 添加事件
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_ALL, const_cast<char*>(menu_items[i].event_tag));

        // 6.1 图标
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, menu_items[i].icon);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);

        // 6.2 文字 (英文 + 中文)
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text_fmt(lbl, "%s\n%s", menu_items[i].text_en, menu_items[i].text_cn);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        
        // 使用全局样式 style_text_cn
        lv_obj_add_style(lbl, &style_text_cn, 0);
    }
}

// ================= 事件回调适配器 =================

// 时间更新回调 (运行在 UI 线程)
static void ui_update_time_async(void* data) {
    // data 是 new 出来的 string，用完要删
    std::string* t_ptr = static_cast<std::string*>(data);
    if (label_time && t_ptr) {
        lv_label_set_text(label_time, t_ptr->c_str());
    }
    delete t_ptr;
}

// 磁盘报警回调
static void ui_update_disk_async(void* data) {
    bool is_full = (bool)(intptr_t)data; // 强转回 bool
    if (label_disk_warn) {
        g_disk_full = is_full;
        if (is_full) lv_obj_remove_flag(label_disk_warn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(label_disk_warn, LV_OBJ_FLAG_HIDDEN);
    }
}

// 摄像头刷新回调
static void ui_update_camera_async(void* /*data*/) {
    // 从 Controller 获取最新帧并拷贝到 cam_buf_display
    // 这里我们依然调用 Controller 的 getDisplayFrame，但它现在只需负责数据拷贝
    // 假设 UiController 内部维护了最新的帧
    // 为了简化，这里我们复用原来的 cam_buf_display 逻辑
    if (lv_screen_active() == screen_main && img_camera) {
         // 从业务层拉取最新数据到 UI 缓存
         UiController::getInstance()->getDisplayFrame(cam_buf_display.data(), CAM_W, CAM_H);
         lv_obj_invalidate(img_camera);
    }
    // 如果是注册页面的摄像头
    if (lv_screen_active() == screen_register && img_face_reg) {
         UiController::getInstance()->getDisplayFrame(cam_buf_display.data(), CAM_W, CAM_H);
         lv_obj_invalidate(img_face_reg);
    }
}

// ================= 事件订阅初始化 =================
static void init_event_subscriptions() {
    auto& bus = EventBus::getInstance();

    // 1. 时间更新
    bus.subscribe(EventType::TIME_UPDATE, [](void* data) {
        // data 是后台线程传来的 string 指针，我们需要拷贝一份传给 UI 线程
        // 或者直接传值。这里演示 copy 方式：
        std::string* new_str = new std::string(*(std::string*)data);
        lv_async_call(ui_update_time_async, new_str);
    });

    // 2. 磁盘状态
    bus.subscribe(EventType::DISK_FULL, [](void*) {
        lv_async_call(ui_update_disk_async, (void*)1);
    });
    bus.subscribe(EventType::DISK_NORMAL, [](void*) {
        lv_async_call(ui_update_disk_async, (void*)0);
    });

    // 3. 摄像头帧
    bus.subscribe(EventType::CAMERA_FRAME_READY, [](void*) {
        lv_async_call(ui_update_camera_async, nullptr);
    });
}

// ================= 页面切换 =================

static void load_main_screen(void) {
    if (!screen_main) create_main_screen();
    
    std::printf("[UI] Switch to Main\n");
    lv_group_remove_all_objs(g_keypad_group);

    lv_screen_load(screen_main);

    destroy_all_screens_except(screen_main);// 销毁其他屏幕，释放内存
    lv_group_add_obj(g_keypad_group, screen_main);
    lv_group_focus_obj(screen_main);
}

static void load_menu_screen(void) {
    if (!screen_menu) create_menu_screen();

    std::printf("[UI] Switch to Menu\n");
    lv_group_remove_all_objs(g_keypad_group);

    lv_screen_load(screen_menu);

    destroy_all_screens_except(screen_menu);// 销毁其他屏幕，释放内存
    // 加入按钮到组
    uint32_t cnt = lv_obj_get_child_cnt(obj_grid);
    for(uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(obj_grid, i);
        lv_group_add_obj(g_keypad_group, child);
    }
    
    // 强制聚焦第一个
    if (cnt > 0) {
        lv_obj_t *first_btn = lv_obj_get_child(obj_grid, 0);
        lv_group_focus_obj(first_btn);
        std::printf("[UI] Initial Focus Set to First Button\n");
    }
    
    // 兜底背景
    lv_group_add_obj(g_keypad_group, screen_menu);
}

// ================= 员工管理子菜单逻辑 =================

// 1. 子菜单事件回调
static void user_mgmt_btn_event_cb(lv_event_t *e) {
    // 获取事件携带的 Tag 参数
    const char* tag = static_cast<const char*>(lv_event_get_user_data(e));
    
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 导航逻辑：1列布局，上下键切换
        lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t *grid = lv_obj_get_parent(btn);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(grid);
        
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(grid, (index + 1) % total));
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(grid, (index + total - 1) % total));
        }
        // 返回键：回到主菜单
        else if (key == LV_KEY_ESC) {
            load_menu_screen(); 
        }
        // 确认键：执行功能
        else if (key == LV_KEY_ENTER) {
            std::printf("[UI] UserMgmt Action: %s\n", tag);
            
            if(std::strcmp(tag, "UserList") == 0) {
                load_user_list_screen();
            }
            else if(std::strcmp(tag, "Register") == 0) {
                // 检查磁盘空间逻辑
                if (g_disk_full) {
                    lv_obj_t * mbox = lv_msgbox_create(NULL);
                    lv_msgbox_add_title(mbox, "Error");
                    lv_msgbox_add_text(mbox, "Disk Full!");
                    lv_obj_t * btn = lv_msgbox_add_footer_button(mbox, "Close");
                    lv_obj_add_event_cb(btn, mbox_close_event_cb, LV_EVENT_CLICKED, mbox);
                    lv_obj_center(mbox);
                } else {
                    screen_user_mgmt = nullptr; // 标记该屏幕即将被销毁
                    load_register_form_screen(); 
                }
            }
            else if(std::strcmp(tag, "DeleteUser") == 0) {
                // [占位] 删除功能提示
                lv_obj_t * mbox = lv_msgbox_create(NULL);
                lv_msgbox_add_title(mbox, "Info");
                lv_msgbox_add_text(mbox, "Delete Feature\nComing Soon...");
                lv_obj_t * btn = lv_msgbox_add_footer_button(mbox, "OK");
                lv_obj_add_event_cb(btn, mbox_close_event_cb, LV_EVENT_CLICKED, mbox);
                lv_obj_center(mbox);
            }
        }
    }
}

// 2. 创建界面 (1列3行)
static void create_user_mgmt_screen(void) {
    screen_user_mgmt = lv_obj_create(nullptr);
    lv_obj_add_style(screen_user_mgmt, &style_base, 0);

    // 标题
    lv_obj_t *title = lv_label_create(screen_user_mgmt);
    lv_label_set_text(title, "User Management / 员工管理");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0);// 应用中文字体样式

    // 布局定义: 1列, 3行 (行高70px)
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {70, 70, 70, LV_GRID_TEMPLATE_LAST}; 

    obj_user_mgmt_grid = lv_obj_create(screen_user_mgmt); 
    lv_obj_set_size(obj_user_mgmt_grid, 220, 240); 
    lv_obj_align(obj_user_mgmt_grid, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_layout(obj_user_mgmt_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_user_mgmt_grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(obj_user_mgmt_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_user_mgmt_grid, 0, 0);
    lv_obj_set_style_pad_row(obj_user_mgmt_grid, 10, 0);

    // 定义菜单项
    struct MenuEntry { const char* icon; const char* en; const char* cn; const char* tag; };
    MenuEntry items[] = {
        {LV_SYMBOL_EDIT,     "User List",    "员工列表", "UserList"},
        {LV_SYMBOL_SETTINGS, "Register",     "员工注册", "Register"},
        {LV_SYMBOL_TRASH,    "Delete User",  "删除员工", "DeleteUser"} 
    };

    for(int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(obj_user_mgmt_grid);
        // 放置在第0列, 第i行
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, i, 1);
        
        // 应用你的红底黄框样式
        lv_obj_add_style(btn, &style_btn_default, 0);
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY); // 键盘聚焦也应用此样式
        lv_obj_add_event_cb(btn, user_mgmt_btn_event_cb, LV_EVENT_ALL, const_cast<char*>(items[i].tag));

        // 布局改为横向 (图标 + 文字)
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(btn, 20, 0);

        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
        
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text_fmt(lbl, "%s  %s", items[i].en, items[i].cn); 
        lv_obj_add_style(lbl, &style_text_cn, 0); // 使用全局样式
    }
}

// 3. 加载函数
static void load_user_mgmt_screen(void) {
    if (!screen_user_mgmt) create_user_mgmt_screen();
    
    std::printf("[UI] Switch to User Mgmt\n");
    lv_group_remove_all_objs(g_keypad_group);
    
    lv_screen_load(screen_user_mgmt); // LVGL 9.x 写法

    destroy_all_screens_except(screen_user_mgmt);// 销毁其他屏幕，释放内存
    // 将按钮加入输入组
    uint32_t cnt = lv_obj_get_child_cnt(obj_user_mgmt_grid);
    for(uint32_t i = 0; i < cnt; i++) {
        lv_group_add_obj(g_keypad_group, lv_obj_get_child(obj_user_mgmt_grid, i));
    }
    // 默认选中第一个
    if(cnt > 0) lv_group_focus_obj(lv_obj_get_child(obj_user_mgmt_grid, 0));
    
    // 背景兜底
    lv_group_add_obj(g_keypad_group, screen_user_mgmt);
}

// ================= 员工注册表单逻辑 =================
// 事件：处理"下一步"按钮点击
static void register_btn_next_event_handler(lv_event_t * e) {
    // 获取传递过来的控件指针
    lv_obj_t * name_ta = (lv_obj_t *)lv_event_get_user_data(e);
    // 我们将 dept_dropdown 指针保存在了 name_ta 的 user_data 中
    lv_obj_t * dept_dd = (lv_obj_t *)lv_obj_get_user_data(name_ta);

    // 1. 获取输入内容
    const char * name_txt = lv_textarea_get_text(name_ta);
    uint16_t selected_dept_idx = lv_dropdown_get_selected(dept_dd);

    // 2. 简单校验：姓名不能为空
    if (strlen(name_txt) == 0) {
        lv_obj_set_style_border_color(name_ta, lv_palette_main(LV_PALETTE_RED), 0);
        return;
    }

    // 3. 获取部门ID (需要重新获取部门列表以匹配索引)
    auto depts = UiController::getInstance()->getDepartmentList();
    if (selected_dept_idx < depts.size()) {
        g_reg_dept_id = depts[selected_dept_idx].id;
    } else {
        g_reg_dept_id = 0; // 默认值
    }

    // 4. 保存姓名 (工号已经在加载页面时通过 get_next_available_id 生成并赋值给了 g_reg_user_id)
    g_reg_name = std::string(name_txt);

    // 5. 跳转到原有的人脸录入界面
    load_register_step();
}
//------------------------------------------------------
// ================= 员工浏览器界面逻辑 =================
//------------------------------------------------------


// ================= Level 2: 员工信息详情 =================

static void load_user_info_screen(int user_id) {
    // 1. 创建屏幕
    if (screen_user_info) lv_obj_delete(screen_user_info);
    screen_user_info = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_user_info, lv_color_hex(0x0F1C2E), 0);// 深蓝背景
    // 2. 获取数据
    UserData u = UiController::getInstance()->getUserInfo(user_id);

    // 3. Grid 容器
    lv_obj_t *grid = lv_obj_create(screen_user_info);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x172A45), 0);// 稍浅的蓝色背景
    lv_obj_set_style_border_width(grid, 0, 0);

    // 布局定义: 2列 (标签列80px, 内容列其余) x 8行
    static int32_t col_dsc[] = {80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {35, 35, 35, 35, 35, 35, 35, 35, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    lv_group_remove_all_objs(g_keypad_group);

    // --- 辅助 Lambda: 创建行 ---
    auto create_row = [&](int row, const char* label, lv_obj_t* content) {
        // Label
        lv_obj_t *lbl = lv_label_create(grid);
        lv_label_set_text(lbl, label);
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0); // 灰色标签
        lv_obj_set_grid_cell(lbl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
        
        // Content
        if (content) {
            lv_obj_set_grid_cell(content, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, row, 1);
            // 如果是可交互对象，添加红色高亮样式
            if (lv_obj_has_flag(content, LV_OBJ_FLAG_CLICKABLE) || lv_obj_check_type(content, &lv_textarea_class)) {
                lv_obj_add_style(content, &style_focus_red, LV_STATE_FOCUSED);
                lv_group_add_obj(g_keypad_group, content);
                
                // 通用 ESC 返回处理
                lv_obj_add_event_cb(content, [](lv_event_t* e){
                    if (lv_event_get_key(e) == LV_KEY_ESC) load_user_list_screen();
                }, LV_EVENT_KEY, nullptr);
            }
        }
        return content;
    };

    // 1. 工号 (Label)
    lv_obj_t *lbl_id = lv_label_create(grid);
    lv_label_set_text_fmt(lbl_id, "%d", u.id);
    lv_obj_set_style_text_color(lbl_id, lv_color_white(), 0);
    create_row(0, "工号", lbl_id);

    // 2. 姓名 (Textarea)
    lv_obj_t *ta_name = lv_textarea_create(grid);
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_text(ta_name, u.name.c_str());
    lv_obj_set_user_data(ta_name, (void*)(intptr_t)u.id);
    // 失去焦点时保存
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e){
         if(lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
             // 简单保存逻辑
             lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
             int uid = (int)(intptr_t)lv_event_get_user_data(e);
             UiController::getInstance()->updateUserName(uid, lv_textarea_get_text(ta));
         }
    }, LV_EVENT_ALL, nullptr);
    create_row(1, "姓名", ta_name);

    // 3. 人脸 (Button)
    lv_obj_t *btn_face = lv_button_create(grid);
    lv_obj_t *lbl_face = lv_label_create(btn_face);
    bool has_face = !u.face_feature.empty();
    lv_label_set_text(lbl_face, has_face ? "已注册 (重录)" : "未注册 (录入)");
    lv_obj_add_style(lbl_face, &style_text_cn, 0);
    lv_obj_center(lbl_face);
    lv_obj_set_user_data(btn_face, (void*)(intptr_t)u.id);
    lv_obj_add_event_cb(btn_face, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            int uid = (int)(intptr_t)lv_event_get_user_data(e);
            // 设置全局注册ID，复用现有的注册逻辑
            g_reg_user_id = uid; 
            g_reg_name = UiController::getInstance()->getUserInfo(uid).name;
            load_register_step(); // 跳转去拍照
        }
    }, LV_EVENT_KEY, nullptr);
    create_row(2, "人脸", btn_face);

    // 4. 部门 (Label)
    lv_obj_t *lbl_dept = lv_label_create(grid);
    lv_label_set_text(lbl_dept, u.dept_name.c_str());
    lv_obj_add_style(lbl_dept, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_dept, lv_color_white(), 0);
    create_row(3, "部门", lbl_dept);

    // 5. 指纹 (Label)
    lv_obj_t *lbl_fp = lv_label_create(grid);
    lv_label_set_text(lbl_fp, "未录入 (暂不支持)");
    lv_obj_add_style(lbl_fp, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_fp, lv_palette_main(LV_PALETTE_GREY), 0);
    create_row(4, "指纹", lbl_fp);

    // 6. 密码 (Button)
    lv_obj_t *btn_pwd = lv_button_create(grid);
    lv_obj_t *lbl_pwd = lv_label_create(btn_pwd);
    bool has_pwd = !u.password.empty();
    lv_label_set_text(lbl_pwd, has_pwd ? "已注册 (修改)" : "未注册 (设置)");
    lv_obj_add_style(lbl_pwd, &style_text_cn, 0);
    lv_obj_center(lbl_pwd);
    lv_obj_set_user_data(btn_pwd, (void*)(intptr_t)u.id);
    lv_obj_add_event_cb(btn_pwd, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            load_password_change_screen((int)(intptr_t)lv_event_get_user_data(e));
        }
    }, LV_EVENT_KEY, nullptr);
    create_row(5, "密码", btn_pwd);

    // 7. 卡号 (Label)
    lv_obj_t *lbl_card = lv_label_create(grid);
    lv_label_set_text(lbl_card, u.card_id.empty() ? "无" : u.card_id.c_str());
    lv_obj_set_style_text_color(lbl_card, lv_color_white(), 0);
    create_row(6, "卡号", lbl_card);

    // 8. 权限 (Button)
    lv_obj_t *btn_role = lv_button_create(grid);
    lv_obj_t *lbl_role = lv_label_create(btn_role);
    lv_label_set_text(lbl_role, (u.role == 1) ? "管理员" : "普通员工");
    lv_obj_add_style(lbl_role, &style_text_cn, 0);
    lv_obj_center(lbl_role);
    
    // 我们需要传递 uid 和 current_role，这里简单用 struct 指针或位运算压缩
    // 简单起见，仅传 uid，进下一级再查一次 role
    lv_obj_set_user_data(btn_role, (void*)(intptr_t)u.id);
    lv_obj_add_event_cb(btn_role, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            int uid = (int)(intptr_t)lv_event_get_user_data(e);
            // 重新获取一下 role 确保准确
            int r = UiController::getInstance()->getUserInfo(uid).role;
            load_role_change_auth_screen(uid, r);
        }
    }, LV_EVENT_KEY, nullptr);
    create_row(7, "权限", btn_role);

    // 4. 显示
    lv_screen_load(screen_user_info);
    destroy_all_screens_except(screen_user_info);
    
    // 默认聚焦姓名
    lv_group_focus_obj(ta_name);
}

// ================= Level 3-A: 修改密码 =================

static void load_password_change_screen(int user_id) {
    if (screen_pwd_change) lv_obj_delete(screen_pwd_change);
    screen_pwd_change = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_pwd_change, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(screen_pwd_change);
    lv_label_set_text(title, "设置密码 / Set Password");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_group_remove_all_objs(g_keypad_group);

    // 容器
    lv_obj_t *cont = lv_obj_create(screen_pwd_change);
    lv_obj_set_size(cont, 220, 200);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);

    // 输入框 1
    lv_obj_t *p1 = lv_textarea_create(cont);
    lv_textarea_set_password_mode(p1, true);
    lv_textarea_set_placeholder_text(p1, "输入新密码");
    lv_textarea_set_one_line(p1, true);
    lv_obj_set_width(p1, LV_PCT(100));
    lv_obj_add_style(p1, &style_focus_red, LV_STATE_FOCUSED);

    // 输入框 2
    lv_obj_t *p2 = lv_textarea_create(cont);
    lv_textarea_set_password_mode(p2, true);
    lv_textarea_set_placeholder_text(p2, "再次输入");
    lv_textarea_set_one_line(p2, true);
    lv_obj_set_width(p2, LV_PCT(100));
    lv_obj_add_style(p2, &style_focus_red, LV_STATE_FOCUSED);

    // 按钮区
    lv_obj_t *btn_box = lv_obj_create(cont);
    lv_obj_set_size(btn_box, LV_PCT(100), 50);
    lv_obj_set_layout(btn_box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_box, LV_OPA_TRANSP, 0);

    // 注册按钮
    lv_obj_t *btn_ok = lv_button_create(btn_box);
    lv_label_set_text(lv_label_create(btn_ok), "注册");
    lv_obj_add_style(btn_ok, &style_focus_red, LV_STATE_FOCUSED);
    lv_obj_add_style(lv_obj_get_child(btn_ok, 0), &style_text_cn, 0);

    // 上下文数据
    struct Ctx { int uid; lv_obj_t *t1; lv_obj_t *t2; };
    Ctx *ctx = new Ctx{user_id, p1, p2}; // 注意：需处理内存释放，此处简化

    // 给 btn_ok 添加销毁回调，当按钮被删除时，自动 delete ctx
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        Ctx* c = (Ctx*)lv_event_get_user_data(e);
        if (c) delete c; // 释放内存
    }, LV_EVENT_DELETE, NULL);

    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            Ctx* c = (Ctx*)lv_event_get_user_data(e);
            const char* s1 = lv_textarea_get_text(c->t1);
            const char* s2 = lv_textarea_get_text(c->t2);
            
            if (strlen(s1) > 0 && strcmp(s1, s2) == 0) {
                UiController::getInstance()->updateUserPassword(c->uid, s1);
                // 提示并返回
                lv_obj_t *m = lv_msgbox_create(NULL);
                lv_msgbox_add_text(m, "注册密码成功");
                lv_msgbox_add_close_button(m);
                lv_obj_center(m);
                // 延时返回逻辑省略，需用户手动关框后按ESC返回
                // 或者直接:
                load_user_info_screen(c->uid);
            } else {
                lv_obj_t *m = lv_msgbox_create(NULL);
                lv_msgbox_add_text(m, "密码不一致或为空");
                lv_msgbox_add_close_button(m);
                lv_obj_center(m);
            }
        }
    }, LV_EVENT_KEY, ctx);
    lv_obj_set_user_data(btn_ok, ctx);

    // 取消按钮
    lv_obj_t *btn_cancel = lv_button_create(btn_box);
    lv_label_set_text(lv_label_create(btn_cancel), "取消");
    lv_obj_add_style(btn_cancel, &style_focus_red, LV_STATE_FOCUSED);
    lv_obj_add_style(lv_obj_get_child(btn_cancel, 0), &style_text_cn, 0);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e){
         if (lv_event_get_key(e) == LV_KEY_ENTER) {
             Ctx* c = (Ctx*)lv_event_get_user_data(e);
             lv_textarea_set_text(c->t1, "");
             lv_textarea_set_text(c->t2, "");
         }
    }, LV_EVENT_KEY, ctx);
    lv_obj_set_user_data(btn_cancel, ctx);

    // 加入组
    lv_group_add_obj(g_keypad_group, p1);
    lv_group_add_obj(g_keypad_group, p2);
    lv_group_add_obj(g_keypad_group, btn_ok);
    lv_group_add_obj(g_keypad_group, btn_cancel);
    lv_group_focus_obj(p1);

    // 全局 ESC
    lv_obj_add_event_cb(p1, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ESC) load_user_info_screen((int)(intptr_t)lv_event_get_user_data(e));
    }, LV_EVENT_KEY, (void*)(intptr_t)user_id);
    // 给其他控件也加 ESC 监听...

    lv_screen_load(screen_pwd_change);
    destroy_all_screens_except(screen_pwd_change);
}

// ================= Level 3-B: 权限变更认证 =================

static void load_role_change_auth_screen(int user_id, int current_role) {
    if (screen_role_auth) lv_obj_delete(screen_role_auth);
    screen_role_auth = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_role_auth, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(screen_role_auth);
    const char* txt = (current_role == 0) ? "录入管理员" : "取消管理员";
    lv_label_set_text(title, txt);
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_group_remove_all_objs(g_keypad_group);

    // 密码框
    lv_obj_t *ta = lv_textarea_create(screen_role_auth);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "请输入管理员密码");
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_width(ta, 200);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_style(ta, &style_focus_red, LV_STATE_FOCUSED);

    // 确认按钮
    lv_obj_t *btn = lv_button_create(screen_role_auth);
    lv_label_set_text(lv_label_create(btn), "确认");
    lv_obj_add_style(lv_obj_get_child(btn, 0), &style_text_cn, 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_style(btn, &style_focus_red, LV_STATE_FOCUSED);

    struct Ctx { int uid; int cur_r; lv_obj_t *ta; };
    Ctx *ctx = new Ctx{user_id, current_role, ta};

    // 添加内存清理回调
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        Ctx* c = (Ctx*)lv_event_get_user_data(e);
        if (c) delete c;
    }, LV_EVENT_DELETE, NULL);

    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            Ctx* c = (Ctx*)lv_event_get_user_data(e);
            const char* pwd = lv_textarea_get_text(c->ta);
            
            // 模拟验证 logic
            if (strcmp(pwd, "123456") == 0) {
                int new_role = (c->cur_r == 0) ? 1 : 0;
                UiController::getInstance()->updateUserRole(c->uid, new_role);
                
                lv_obj_t *m = lv_msgbox_create(NULL);
                lv_msgbox_add_text(m, "操作成功");
                lv_msgbox_add_close_button(m);
                lv_obj_center(m);
                
                // 返回详情页
                load_user_info_screen(c->uid);
            } else {
                lv_obj_t *m = lv_msgbox_create(NULL);
                lv_msgbox_add_text(m, "密码错误");
                lv_msgbox_add_close_button(m);
                lv_obj_center(m);
                lv_textarea_set_text(c->ta, "");
            }
        }
    }, LV_EVENT_KEY, ctx);
    lv_obj_set_user_data(btn, ctx);

    // 导航 ESC
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ESC) load_user_info_screen((int)(intptr_t)lv_event_get_user_data(e));
    }, LV_EVENT_KEY, (void*)(intptr_t)user_id);

    lv_group_add_obj(g_keypad_group, ta);
    lv_group_add_obj(g_keypad_group, btn);
    lv_group_focus_obj(ta);

    lv_screen_load(screen_role_auth);
    destroy_all_screens_except(screen_role_auth);
}

// ================= [Epic 5.2] 记录查询功能实现 =================

// 查询界面通用事件回调 (处理导航和动作)
static void query_screen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // --- 如果焦点在输入框 ---
        if (target == ta_query_id) {
            if (key == LV_KEY_ENTER) {
                const char *txt = lv_textarea_get_text(ta_query_id);
                if (txt && strlen(txt) > 0) {
                    int uid = std::atoi(txt);
                    load_record_result_screen(uid); // 执行查询
                }
            }
            else if (key == LV_KEY_DOWN) {
                // 向下切换到返回按钮
                lv_group_focus_obj(btn_query_back);
            }
            else if (key == LV_KEY_ESC) {
                load_menu_screen();
            }
        }
        // --- 如果焦点在返回按钮 ---
        else if (target == btn_query_back) {
            if (key == LV_KEY_ENTER) {
                load_menu_screen(); // 执行返回
            }
            else if (key == LV_KEY_UP) {
                // 向上切换回输入框
                lv_group_focus_obj(ta_query_id);
            }
            else if (key == LV_KEY_ESC) {
                load_menu_screen();
            }
        }
    }
    // 处理点击事件 (兼顾鼠标/触摸)
    else if (code == LV_EVENT_CLICKED) {
        if (target == btn_query_back) {
            load_menu_screen();
        }
    }
}

static void create_record_query_screen(void) {
    if (screen_rec_query) return;

    screen_rec_query = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_rec_query, lv_color_black(), 0);

    // 1. 标题
    lv_obj_t *title = lv_label_create(screen_rec_query);
    lv_label_set_text(title, "Record Query / 记录查询");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0); // 应用中文字体样式

    // 2. 提示文本
    lv_obj_t *label = lv_label_create(screen_rec_query);
    lv_label_set_text(label, "Enter User ID:");
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);

    // 3. 输入框
    ta_query_id = lv_textarea_create(screen_rec_query);
    lv_textarea_set_one_line(ta_query_id, true);
    lv_textarea_set_max_length(ta_query_id, 5);
    lv_textarea_set_accepted_chars(ta_query_id, "0123456789");
    lv_obj_set_width(ta_query_id, 140);
    lv_obj_align(ta_query_id, LV_ALIGN_CENTER, 0, -10);
    // 绑定事件
    lv_obj_add_event_cb(ta_query_id, query_screen_event_cb, LV_EVENT_ALL, nullptr);

    // 4. [新增] 返回按钮
    btn_query_back = lv_button_create(screen_rec_query);
    lv_obj_set_size(btn_query_back, 100, 40);
    lv_obj_align(btn_query_back, LV_ALIGN_CENTER, 0, 60); // 放在输入框下方
    lv_obj_set_style_bg_color(btn_query_back, lv_color_hex(0x444444), 0);
    
    // 按钮焦点样式 (红底黄框)
    lv_obj_add_style(btn_query_back, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_query_back, &style_btn_focused, LV_STATE_FOCUS_KEY);

    // 按钮文字
    lv_obj_t *lbl_back = lv_label_create(btn_query_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(lbl_back);
    
    // 绑定事件
    lv_obj_add_event_cb(btn_query_back, query_screen_event_cb, LV_EVENT_ALL, nullptr);
    
    // 5. 底部操作提示
    lv_obj_t *tip = lv_label_create(screen_rec_query);
    lv_label_set_text(tip, "Enter: Search / Select");
    lv_obj_set_style_text_color(tip, lv_color_hex(0x888888), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static void load_record_query_screen(void) {
    if (!screen_rec_query) create_record_query_screen();
    
    // 重置输入框
    lv_textarea_set_text(ta_query_id, "");
    
    // 重建焦点组
    lv_group_remove_all_objs(g_keypad_group);
    
    lv_screen_load(screen_rec_query);
    
    // 将 输入框 和 返回按钮 都加入组
    lv_group_add_obj(g_keypad_group, ta_query_id);
    lv_group_add_obj(g_keypad_group, btn_query_back);
    
    // 默认聚焦输入框
    lv_group_focus_obj(ta_query_id);

    destroy_all_screens_except(screen_rec_query);// 销毁其他屏幕，释放内存
}

// =================================================================
// 员工列表页实现代码
// =================================================================

// 列表按钮的事件回调 (处理上下滚动和返回)
static void list_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 向下键：移动到下一个
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(g_keypad_group);
        }
        // 向上键：移动到上一个
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(g_keypad_group);
        }
        // ESC键：如果当前在列表页，按ESC返回员工管理页
        else if (key == LV_KEY_ESC) {
            // 如果当前是“员工列表页”，则返回“员工管理菜单”
            if (lv_screen_active() == screen_list) {
                load_user_mgmt_screen(); 
            }
            // 否则（比如是“打卡记录页”），必须返回“主菜单”
            else {
                load_menu_screen(); 
            }
        }
        // ENTER键：查看详情 (目前仅打印)
        else if (key == LV_KEY_ENTER) {
            std::printf("[UI] View User Details...\n");
        }
    }
}

// 创建列表屏幕 UI
static void user_list_item_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    // 获取触发事件的对象 (按钮)，而不是事件数据
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        // 按下 Enter 进入详情页 (Level 2)
        if (key == LV_KEY_ENTER) {
            // 从按钮对象中取出 user_id
            int user_id = (int)(intptr_t)lv_obj_get_user_data(btn);
            
            std::printf("[UI] Click User ID: %d\n", user_id); // 添加调试打印
            load_user_info_screen(user_id);
        }
        // 按下 ESC 返回员工管理菜单
        else if (key == LV_KEY_ESC) {
            load_user_mgmt_screen();
        }
        // 上下导航
        else if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(g_keypad_group);
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(g_keypad_group);
        }
    }
    // 增加点击支持 (触摸/鼠标)
    else if (code == LV_EVENT_CLICKED) {
         int user_id = (int)(intptr_t)lv_obj_get_user_data(btn);
         load_user_info_screen(user_id);
    }
}

// --- 辅助：关闭 Msgbox 的通用回调 ---
static void mbox_close_event_cb(lv_event_t * e) {
    // user_data 传入的是 mbox 对象
    lv_obj_t * mbox = (lv_obj_t *)lv_event_get_user_data(e);
    if(mbox) lv_msgbox_close(mbox);
}

// --- 动作：确认清空记录 ---
static void op_clear_confirm_cb(lv_event_t * e) {
    lv_obj_t * mbox = (lv_obj_t *)lv_event_get_user_data(e);
    
    // 1. 执行业务
    UiController::getInstance()->clearAllRecords();
    
    // 2. 关闭确认框
    if(mbox) lv_msgbox_close(mbox);
    
    // 3. 弹出成功提示 (v9写法)
    lv_obj_t * info = lv_msgbox_create(NULL);
    lv_msgbox_add_title(info, "System");
    lv_msgbox_add_text(info, "All records cleared!");
    lv_obj_t * btn = lv_msgbox_add_footer_button(info, "OK");
    lv_obj_add_event_cb(btn, mbox_close_event_cb, LV_EVENT_CLICKED, info);
    lv_obj_center(info);
}

// --- 动作：确认恢复出厂 ---
static void op_reset_confirm_cb(lv_event_t * e) {
    lv_obj_t * mbox = (lv_obj_t *)lv_event_get_user_data(e);
    
    UiController::getInstance()->factoryReset();
    
    if(mbox) lv_msgbox_close(mbox);
    request_exit(); // 重置后退出程序
}

// ================= [System Settings] 系统设置界面实现 =================

// --- Level 2: 高级设置 (二级菜单) ---

static void sys_adv_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *grid = lv_obj_get_parent(btn);
    const char* tag = (const char*)lv_event_get_user_data(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // --- 核心修复：手动处理上下键导航 ---
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(grid);

        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(grid, (index + 1) % total));
        }
        else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(grid, (index + total - 1) % total));
        }
        // ------------------------------------

        else if (key == LV_KEY_ESC) {
            load_sys_settings_screen(); // 返回一级菜单
        }

        else if (key == LV_KEY_ENTER) {
            // 执行具体的清除逻辑
            if (std::strcmp(tag, "CLR_REC") == 0) on_clear_all_records();
            else if (std::strcmp(tag, "CLR_EMP") == 0) on_clear_all_employees();
            else if (std::strcmp(tag, "CLR_DATA") == 0) on_clear_all_data();
            else if (std::strcmp(tag, "RESET") == 0) on_factory_reset();
            else {
                 // 提示占位功能
                lv_obj_t * mbox = lv_msgbox_create(NULL);
                lv_msgbox_add_text(mbox, "Function Pending");
                lv_msgbox_add_close_button(mbox);
                lv_obj_center(mbox);
            }
        }
    }
}

static void create_sys_adv_screen() {
    screen_sys_adv = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_sys_adv, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(screen_sys_adv);
    lv_label_set_text(title, "Advanced Settings / 高级设置");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0);// 应用中文字体样式

    static int32_t col_dsc[] = {220, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {45, 45, 45, 45, 45, LV_GRID_TEMPLATE_LAST}; 

    // [Fix] 使用全局变量 obj_adv_grid
    obj_adv_grid = lv_obj_create(screen_sys_adv);
    lv_obj_set_size(obj_adv_grid, 230, 260);
    lv_obj_align(obj_adv_grid, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_layout(obj_adv_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_adv_grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(obj_adv_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_adv_grid, 0, 0);
    lv_obj_set_style_pad_row(obj_adv_grid, 5, 0); 

    // 使用正确的图标 LV_SYMBOL_DIRECTORY
    create_sys_grid_btn(obj_adv_grid, 0, LV_SYMBOL_LIST,      "Clear Records",   "清除所有记录", sys_adv_event_cb, "CLR_REC");
    create_sys_grid_btn(obj_adv_grid, 1, LV_SYMBOL_DIRECTORY, "Clear Employees", "清除所有员工", sys_adv_event_cb, "CLR_EMP");
    create_sys_grid_btn(obj_adv_grid, 2, LV_SYMBOL_TRASH,     "Clear All Data",  "清除所有数据", sys_adv_event_cb, "CLR_DATA");
    create_sys_grid_btn(obj_adv_grid, 3, LV_SYMBOL_SETTINGS,  "Factory Reset",   "恢复出厂设置", sys_adv_event_cb, "RESET");
    create_sys_grid_btn(obj_adv_grid, 4, LV_SYMBOL_UPLOAD,    "System Upgrade",  "系统升级",    sys_adv_event_cb, "UPGRADE");
}

static void load_sys_adv_screen(void) {
    if (!screen_sys_adv) create_sys_adv_screen();
    
    std::printf("[UI] Enter: System Advanced Settings\n");
    
    lv_group_remove_all_objs(g_keypad_group);
    
    // [Fix] 直接使用 obj_adv_grid
    if (obj_adv_grid) {
        uint32_t cnt = lv_obj_get_child_cnt(obj_adv_grid);
        for(uint32_t i=0; i<cnt; i++) {
            lv_group_add_obj(g_keypad_group, lv_obj_get_child(obj_adv_grid, i));
        }
        if(cnt > 0) lv_group_focus_obj(lv_obj_get_child(obj_adv_grid, 0));
    }

    lv_screen_load(screen_sys_adv);
    destroy_all_screens_except(screen_sys_adv);// 销毁其他屏幕，释放内存
}

// --- Level 1: 系统设置 (一级菜单) ---

static void sys_settings_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    // 获取当前按钮和它的父容器（Grid）
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *grid = lv_obj_get_parent(btn);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // --- 核心修复：手动处理上下键导航 ---
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(grid);

        if (key == LV_KEY_DOWN) {
            // 向下切换：(当前 + 1) % 总数
            lv_group_focus_obj(lv_obj_get_child(grid, (index + 1) % total));
        }
        else if (key == LV_KEY_UP) {
            // 向上切换：(当前 + 总数 - 1) % 总数
            lv_group_focus_obj(lv_obj_get_child(grid, (index + total - 1) % total));
        }
        // ------------------------------------

        // 返回键
        else if (key == LV_KEY_ESC) {
            load_menu_screen(); // 回到九宫格主菜单
        }
        // 确认键
        else if (key == LV_KEY_ENTER) {
            std::printf("[System] Select: %s\n", tag);
            
            // 给用户一点视觉反馈（左右震动），表示按键生效了
            // 注意：lv_obj_shake_x 需要 lvgl extra 模块，如果没有开启，可以注释掉下面这行
            // lv_obj_shake_x(btn); 

            if (std::strcmp(tag, "ADVANCED") == 0) {
                load_sys_adv_screen(); // 进入二级界面
            }
            else {
                // 对于占位功能，弹出一个提示框，让你知道按键是好使的
                lv_obj_t * mbox = lv_msgbox_create(NULL);
                lv_msgbox_add_title(mbox, "Info");
                lv_msgbox_add_text(mbox, "Feature Coming Soon...");
                lv_msgbox_add_close_button(mbox);
                lv_obj_center(mbox);
            }
        }
    }
}

// ================= [System Info] 业务与界面实现 =================

// 1. 数据统计辅助函数
struct StorageStats {
    int total_users;
    int admin_count;
    int pwd_users;
    int record_count;
};

static StorageStats get_storage_statistics() {
    StorageStats stats = {0, 0, 0, 0};
    
    // A. 统计人员信息
    int count = UiController::getInstance()->getUserCount();
    stats.total_users = count;
    
    // 遍历检查管理员和密码 (虽然有点笨，但这是不修改 DB 接口的最快方法)
    char name_buf[64];
    int uid = 0;
    for(int i=0; i<count; i++) {
        // 注意：这里假设 business_get_user_at 只返回 ID 和 Name
        // 为了统计 role 和 password，我们需要查 DB 详情
        if(UiController::getInstance()->getUserAt(i, &uid, name_buf, sizeof(name_buf))) {
             UserData u = UiController::getInstance()->getUserInfo(uid);
             if (u.role == 1) stats.admin_count++;
             if (!u.password.empty()) stats.pwd_users++;
        }
    }
    
    // B. 统计记录数 (获取大范围记录并计数)
    // 实际项目中建议在 DB 层增加 db_get_record_count() 接口
    std::time_t now = std::time(nullptr);
    auto recs = UiController::getInstance()->getRecords(-1, 0, now + 864000); // 获取所有
    stats.record_count = (int)recs.size();
    
    return stats;
}

// 2. Level 2: 存储信息详情页 (Storage Info)
static void info_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        load_sys_info_screen(); // 返回上一级
    }
}

static void create_storage_info_screen() {
    screen_storage_info = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_storage_info, lv_color_black(), 0);
    
    // 标题
    lv_obj_t *title = lv_label_create(screen_storage_info);
    lv_label_set_text(title, "Storage Statistics");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0);// 应用中文字体样式

    // 获取数据
    StorageStats s = get_storage_statistics();

    // 创建展示容器
    lv_obj_t *cont = lv_obj_create(screen_storage_info);
    lv_obj_set_size(cont, 220, 200);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 15, 0);
    lv_obj_set_style_pad_gap(cont, 15, 0);

    // 辅助宏：创建一行信息 "Label: Value"
    auto add_item = [&](const char* cn_label, int value, lv_color_t color) {
        lv_obj_t *item = lv_obj_create(cont);
        lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, 0);
        
        lv_obj_t *lbl = lv_label_create(item);
        lv_label_set_text_fmt(lbl, "%s: %d", cn_label, value);
        lv_obj_add_style(lbl, &style_text_cn, 0);// 中文字体样式
        lv_obj_set_style_text_color(lbl, color, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    };

    add_item("员工注册数", s.total_users, THEME_COLOR_PRIMARY);
    add_item("管理员数",   s.admin_count, lv_palette_main(LV_PALETTE_ORANGE));
    add_item("密码注册数", s.pwd_users,   lv_palette_main(LV_PALETTE_PURPLE));
    add_item("总记录数",   s.record_count,lv_palette_main(LV_PALETTE_GREEN));
    
    // 绑定 ESC 返回事件到容器（因为这里没有按钮，需要容器接收键盘事件）
    lv_obj_add_event_cb(screen_storage_info, info_back_event_cb, LV_EVENT_KEY, nullptr);
    lv_group_add_obj(g_keypad_group, screen_storage_info);
}

static void load_storage_info_screen() {
    // 每次加载都重新创建，以刷新数据
    if (screen_storage_info) lv_obj_delete(screen_storage_info);
    create_storage_info_screen();
    
    std::printf("[UI] Enter: Storage Info\n");
    lv_group_remove_all_objs(g_keypad_group);
    lv_group_add_obj(g_keypad_group, screen_storage_info);
    lv_group_focus_obj(screen_storage_info);
    
    lv_screen_load(screen_storage_info);
    destroy_all_screens_except(screen_storage_info);// 销毁其他屏幕，释放内存
}

// 3. Level 1: 系统信息菜单 (System Info Menu)
static void sys_info_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *grid = lv_obj_get_parent(btn);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 上下导航逻辑
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(grid);
        if (key == LV_KEY_DOWN) lv_group_focus_obj(lv_obj_get_child(grid, (index + 1) % total));
        else if (key == LV_KEY_UP) lv_group_focus_obj(lv_obj_get_child(grid, (index + total - 1) % total));
        
        else if (key == LV_KEY_ESC) load_menu_screen(); // 返回主菜单
        
        else if (key == LV_KEY_ENTER) {
            if (std::strcmp(tag, "STORAGE") == 0) {
                load_storage_info_screen();
            } else if (std::strcmp(tag, "HARDWARE") == 0) {
                // 仅占位
                lv_obj_t * mbox = lv_msgbox_create(NULL);
                lv_msgbox_add_text(mbox, "Hardware Info\n(Coming Soon)");
                lv_msgbox_add_close_button(mbox);
                lv_obj_center(mbox);
            }
        }
    }
}

static void create_sys_info_screen() {
    screen_sys_info = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_sys_info, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(screen_sys_info);
    lv_label_set_text(title, "System Info / 系统信息");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0);// 应用中文字体样式

    // 1列 2行 Grid
    static int32_t col_dsc[] = {220, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {60, 60, LV_GRID_TEMPLATE_LAST}; 

    obj_info_grid = lv_obj_create(screen_sys_info);
    lv_obj_set_size(obj_info_grid, 230, 180);
    lv_obj_align(obj_info_grid, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_layout(obj_info_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_info_grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(obj_info_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_info_grid, 0, 0);
    lv_obj_set_style_pad_row(obj_info_grid, 15, 0);

    // 添加两个按钮
    create_sys_grid_btn(obj_info_grid, 0, LV_SYMBOL_DRIVE, "Storage Info", "存储信息", sys_info_event_cb, "STORAGE");
    create_sys_grid_btn(obj_info_grid, 1, LV_SYMBOL_SD_CARD, "Hardware Info", "硬件信息", sys_info_event_cb, "HARDWARE");
}

static void load_sys_info_screen(void) {
    if (!screen_sys_info) create_sys_info_screen();
    std::printf("[UI] Enter: System Info Menu\n");

    lv_group_remove_all_objs(g_keypad_group);
    if (obj_info_grid) {
        uint32_t cnt = lv_obj_get_child_cnt(obj_info_grid);
        for(uint32_t i=0; i<cnt; i++) lv_group_add_obj(g_keypad_group, lv_obj_get_child(obj_info_grid, i));
        if (cnt > 0) lv_group_focus_obj(lv_obj_get_child(obj_info_grid, 0));
    }
    lv_screen_load(screen_sys_info);
    destroy_all_screens_except(screen_sys_info);// 销毁其他屏幕，释放内存
}

static void create_sys_settings_screen() {
    screen_sys_settings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_sys_settings, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(screen_sys_settings);
    lv_label_set_text(title, "System Settings / 系统设置");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0);// 应用中文字体样式

    // 布局: 1列 3行
    static int32_t col_dsc[] = {220, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {60, 60, 60, LV_GRID_TEMPLATE_LAST}; 

    // [Fix] 使用全局变量 obj_sys_grid
    obj_sys_grid = lv_obj_create(screen_sys_settings);
    lv_obj_set_size(obj_sys_grid, 230, 220);
    lv_obj_align(obj_sys_grid, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_layout(obj_sys_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_sys_grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(obj_sys_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_sys_grid, 0, 0);
    lv_obj_set_style_pad_row(obj_sys_grid, 10, 0);

    create_sys_grid_btn(obj_sys_grid, 0, LV_SYMBOL_SETTINGS, "Basic Settings",     "基础设置", sys_settings_event_cb, "BASIC");
    create_sys_grid_btn(obj_sys_grid, 1, LV_SYMBOL_EDIT,     "Advanced Settings",  "高级设置", sys_settings_event_cb, "ADVANCED");
    create_sys_grid_btn(obj_sys_grid, 2, LV_SYMBOL_REFRESH,  "Self-check",         "自检功能", sys_settings_event_cb, "CHECK");
}

// 保持接口兼容：旧的 load_sys_ops_screen 现在直接调用新的加载函数
static void load_sys_ops_screen(void) {
    load_sys_settings_screen();
}

static void load_sys_settings_screen(void) {
    if (!screen_sys_settings) create_sys_settings_screen();

    std::printf("[UI] Enter: System Settings (Level 1)\n");

    // 1. 清空当前按键组
    lv_group_remove_all_objs(g_keypad_group);
    
    // 2. [Fix] 直接使用 obj_sys_grid 全局变量，确保获取到正确的容器
    if (obj_sys_grid) {
        uint32_t cnt = lv_obj_get_child_cnt(obj_sys_grid);
        for(uint32_t i=0; i<cnt; i++) {
            lv_group_add_obj(g_keypad_group, lv_obj_get_child(obj_sys_grid, i));
        }
        
        // 默认聚焦第二个 "高级设置"
        if(cnt > 1) lv_group_focus_obj(lv_obj_get_child(obj_sys_grid, 1)); 
        else if(cnt > 0) lv_group_focus_obj(lv_obj_get_child(obj_sys_grid, 0));
    }

    lv_screen_load(screen_sys_settings);
    destroy_all_screens_except(screen_sys_settings);// 清理其他屏幕资源
}

//  加载数据并显示
static void load_user_list_screen(void) {
// 1. 创建屏幕 (如果未创建)
    if (!screen_list) {
        screen_list = lv_obj_create(nullptr);
        
        //  主背景设为 深邃午夜蓝 (#0F1C2E)
        lv_obj_set_style_bg_color(screen_list, lv_color_hex(0x0F1C2E), 0); 
        
        lv_obj_t *title = lv_label_create(screen_list);
        lv_label_set_text(title, "员工列表 / User List");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_add_style(title, &style_text_cn, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

        // 创建列表容器
        obj_list_view = lv_obj_create(screen_list);
        lv_obj_set_size(obj_list_view, LV_PCT(95), LV_PCT(80));
        lv_obj_align(obj_list_view, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_flex_flow(obj_list_view, LV_FLEX_FLOW_COLUMN); 

        // 列表容器设为 你选择的稍亮蓝灰 (#172A45)
        lv_obj_set_style_bg_color(obj_list_view, lv_color_hex(0x172A45), 0);
        
        // 建议：移除容器的边框，让色块更纯净
        lv_obj_set_style_border_width(obj_list_view, 0, 0);

        lv_obj_set_style_pad_all(obj_list_view, 5, 0);
        lv_obj_set_style_pad_gap(obj_list_view, 5, 0); 
    }

    // 2. 清理旧数据并准备输入组
    lv_obj_clean(obj_list_view); 
    lv_group_remove_all_objs(g_keypad_group);

    // 3. 获取所有用户数据
    // 注意：这里假设 Controller 有这个方法，如果没有，请参考 ui_controller.h 修改建议
    std::vector<UserData> users = UiController::getInstance()->getAllUsers();

    if (users.empty()) {
        lv_obj_t *lbl = lv_label_create(obj_list_view);
        lv_label_set_text(lbl, "暂无员工数据");
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    } else {
        for (const auto& u : users) {
            // 创建列表项按钮
            lv_obj_t *btn = lv_button_create(obj_list_view);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, 45); // 稍微加高一点
            lv_obj_add_style(btn, &style_btn_default, 0);

            // 布局：左中右
            lv_obj_t *lbl = lv_label_create(btn);
            
            // 确保 dept_name 不为空，如果为空手动赋值，避免显示异常
            std::string d_name = u.dept_name.empty() ? "-" : u.dept_name;
            
            // 使用固定宽度格式化，或者简单的 | 分隔
            // 格式： ID  |  Name  |  Dept
            lv_label_set_text_fmt(lbl, "%d  |  %s  |  %s", u.id, u.name.c_str(), d_name.c_str());
            
            lv_obj_add_style(lbl, &style_text_cn, 0);
            lv_obj_center(lbl);

            // 绑定数据和事件
            lv_obj_set_user_data(btn, (void*)(intptr_t)u.id);
            lv_obj_add_event_cb(btn, user_list_item_event_cb, LV_EVENT_KEY, nullptr);
            
            // 加入键盘组
            lv_group_add_obj(g_keypad_group, btn);
        }
        // 聚焦第一个
        lv_group_focus_obj(lv_obj_get_child(obj_list_view, 0));
    }

    // 4. 显示
    lv_screen_load(screen_list);
    destroy_all_screens_except(screen_list);
    
    // 兜底，防止空列表导致按键失效
    lv_group_add_obj(g_keypad_group, screen_list);
}

// =================================================================
// [Epic 3.3] 注册向导实现 (Wizard Implementation)
// =================================================================

// --- Step 2 事件: 处理按键 (拍照/返回) ---
static void reg_step2_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_ENTER) {
            // [核心] 触发业务层采集
            std::printf("[UI] Capturing face for: %s\n", g_reg_name.c_str());
            
            // 调用业务接口保存
            if (UiController::getInstance()->registerNewUser(g_reg_name.c_str(), g_reg_dept_id)) {
                std::printf("[UI] Reg Success! Back to Menu.\n");
                
                // 简单反馈：延迟或直接返回
                load_user_mgmt_screen(); 
            } else {
                // 错误提示弹窗
                std::printf("[UI] Reg Failed! Check DB constraints.\n");
                lv_obj_t * mbox = lv_msgbox_create(NULL);
                lv_msgbox_add_text(mbox, "Registration Failed!\n(Check DB/Dept ID)");
                lv_msgbox_add_close_button(mbox);
                lv_obj_center(mbox);
            }
        }
        else if (key == LV_KEY_ESC) {
            load_register_form_screen();
        }
    }
}

// --- 创建注册屏幕容器 ---
static void create_register_screen(void) {
    if (screen_register) return;
    screen_register = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_register, lv_color_black(), 0);
}

/* --- ：强制导航模式的定时器回调 --- */
static void force_nav_mode_timer_cb(lv_timer_t * t) {
    if (g_keypad_group) {
        lv_group_set_editing(g_keypad_group, false); // 再次强制设为导航模式
        std::printf("[Debug] Timer: Enforced Nav Mode (Editing=False)\n");
    }
    lv_timer_del(t); // 任务完成，自杀
}

/* --- 表单导航专用回调：实现按上下键切换焦点 --- */
static void form_nav_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);

    // [1] 焦点进入时：启动定时器强制修正状态
    if (code == LV_EVENT_FOCUSED) {
        // 给个亮橙色边框，确信焦点真的在这
        lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_border_width(obj, 3, 0);

        if (g_keypad_group) {
            // 如果是下拉框或按钮，我们要强制它处于“导航模式”
            if (lv_obj_check_type(obj, &lv_dropdown_class) || 
                lv_obj_check_type(obj, &lv_button_class)) {
                
                // 【核心修复】先设置一次，并启动定时器在下一帧再设置一次
                lv_group_set_editing(g_keypad_group, false);
                lv_timer_create(force_nav_mode_timer_cb, 10, NULL); 
            }
            // 输入框则保持编辑模式
            else if (lv_obj_check_type(obj, &lv_textarea_class)) {
                lv_group_set_editing(g_keypad_group, true);
            }
        }
    }
    // [2] 失去焦点：恢复样式
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_width(obj, 1, 0);
        lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    // [3] 按键处理
    else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        bool is_expanded = false;
        if (lv_obj_check_type(obj, &lv_dropdown_class)) {
            is_expanded = lv_dropdown_is_open(obj);
        }

        // 只要下拉框没展开，就拦截跳转
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            if (!is_expanded) {
                if (g_keypad_group) lv_group_set_editing(g_keypad_group, false);
                lv_group_focus_next(g_keypad_group);
            }
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            if (!is_expanded) {
                std::printf("[Debug] UP Key Pressed -> Jumping Prev\n");
                if (g_keypad_group) lv_group_set_editing(g_keypad_group, false);
                lv_group_focus_prev(g_keypad_group);
            }
        }
        else if (key == LV_KEY_ENTER) {
            if (lv_obj_check_type(obj, &lv_textarea_class)) {
                if (g_keypad_group) lv_group_set_editing(g_keypad_group, false);
                lv_group_focus_next(g_keypad_group);
            }
        }
    }
}

//---  创建整合后的注册表单界面 --- 
void load_register_form_screen() {
    // 1.  如果全局变量已存在，先销毁旧的，确保重新创建
    if (screen_register) lv_obj_delete(screen_register);
    
    // 2. 创建新屏幕并赋值给全局变量
    screen_register = lv_obj_create(NULL);
    
    // 3. 让局部指针 screen 指向全局变量
    lv_obj_t * screen = screen_register; 

    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF0F0F0), 0);

    lv_obj_t * title = lv_label_create(screen);
    lv_label_set_text(title, "员工注册 / Registration");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    // 标题应用中文字体样式
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0); 

    lv_obj_t * form_cont = lv_obj_create(screen);
    lv_obj_set_size(form_cont, 220, 180); 
    lv_obj_align(form_cont, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(form_cont, LV_FLEX_FLOW_COLUMN); 
    lv_obj_set_style_pad_all(form_cont, 5, 0);

    // --- ID ---
    lv_obj_t * lbl_id = lv_label_create(form_cont); // 捕获指针以便将来扩展
    lv_label_set_text(lbl_id, "ID (Auto):");
    
    lv_obj_t * ta_id = lv_textarea_create(form_cont);
    lv_obj_set_width(ta_id, LV_PCT(100));
    lv_textarea_set_one_line(ta_id, true);
    lv_obj_add_state(ta_id, LV_STATE_DISABLED);
    
    int new_id = UiController::getInstance()->generateNextUserId();
    g_reg_user_id = new_id;
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "%06d", new_id);
    lv_textarea_set_text(ta_id, buf);

    // --- Name ---
    // 姓名标签
    lv_obj_t * lbl_name = lv_label_create(form_cont);
    lv_label_set_text(lbl_name, "Name / 姓名:");
    lv_obj_add_style(lbl_name, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_name, lv_color_black(), 0);

    lv_obj_t * ta_name = lv_textarea_create(form_cont);
    lv_obj_set_width(ta_name, LV_PCT(100));
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_placeholder_text(ta_name, "Enter Name");
    lv_obj_add_event_cb(ta_name, form_nav_event_cb, LV_EVENT_ALL, NULL);
    
    // --- Dept ---
    //部门标签
    lv_obj_t * lbl_dept = lv_label_create(form_cont);
    lv_label_set_text(lbl_dept, "Dept / 部门:");
    lv_obj_add_style(lbl_dept, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_dept, lv_color_black(), 0);

    // 部门下拉框
    lv_obj_t * dd_dept = lv_dropdown_create(form_cont);
    lv_obj_set_width(dd_dept, LV_PCT(100));
    lv_obj_remove_flag(dd_dept, LV_OBJ_FLAG_SCROLLABLE);

    // 填充数据
    auto depts = UiController::getInstance()->getDepartmentList();
    std::string opts = "";
    for (const auto& d : depts) {
        if (!opts.empty()) opts += "\n";
        opts += d.name;
    }
    if (opts.empty()) opts = "Default";
    lv_dropdown_set_options(dd_dept, opts.c_str());
    
    // 给下拉框列表也加个保险（虽然下拉框自带样式比较复杂，但文字部分可以尝试应用）
    lv_obj_add_style(dd_dept, &style_text_cn, 0);
    lv_obj_set_style_text_color(dd_dept, lv_color_black(), 0);
    lv_obj_t * list = lv_dropdown_get_list(dd_dept);
    if (list) {
        lv_obj_add_style(list, &style_text_cn, 0); // 列表也需要中文字体
        lv_obj_set_style_text_color(list, lv_color_black(), 0); // 列表文字也要黑色
    }
    lv_obj_add_event_cb(dd_dept, form_nav_event_cb, LV_EVENT_ALL, NULL);

    // --- Buttons ---
    lv_obj_t * btn_area = lv_obj_create(screen);
    lv_obj_remove_style_all(btn_area);
    lv_obj_set_size(btn_area, LV_PCT(100), 40);
    lv_obj_align(btn_area, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_set_flex_flow(btn_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_area, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_cancel = lv_button_create(btn_area);
    lv_obj_set_width(btn_cancel, 80);
    lv_obj_set_style_bg_color(btn_cancel, lv_palette_main(LV_PALETTE_RED), 0);
    lv_label_set_text(lv_label_create(btn_cancel), "Cancel");
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t * e){ load_user_mgmt_screen(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_cancel, form_nav_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * btn_next = lv_button_create(btn_area);
    lv_obj_set_width(btn_next, 80);
    lv_obj_set_style_bg_color(btn_next, THEME_COLOR_PRIMARY, 0);
    lv_label_set_text(lv_label_create(btn_next), "Next >");
    lv_obj_set_user_data(ta_name, dd_dept);
    lv_obj_add_event_cb(btn_next, register_btn_next_event_handler, LV_EVENT_CLICKED, ta_name);
    lv_obj_add_event_cb(btn_next, form_nav_event_cb, LV_EVENT_ALL, NULL);

    // --- Footer ---
    lv_obj_t * bottom_bar = lv_obj_create(screen);
    lv_obj_set_size(bottom_bar, LV_PCT(100), 30);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_black(), 0);
    lv_label_set_text(lv_label_create(bottom_bar), "Enter:Select  Up/Down:Nav");
    lv_obj_set_style_text_color(lv_obj_get_child(bottom_bar, 0), lv_color_white(), 0);
    lv_obj_center(lv_obj_get_child(bottom_bar, 0));

    // --- Group ---
    lv_group_remove_all_objs(g_keypad_group);
    lv_group_add_obj(g_keypad_group, ta_name);
    lv_group_add_obj(g_keypad_group, dd_dept);
    lv_group_add_obj(g_keypad_group, btn_next);
    lv_group_add_obj(g_keypad_group, btn_cancel);

    lv_group_focus_obj(ta_name);

    //加载屏幕 (建议显式使用全局变量)
    lv_screen_load_anim(screen_register, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
    //销毁除了自己以外的所有屏幕
    destroy_all_screens_except(screen_register);
}

// --- 加载 Step 2: 采集人脸 ---
static void load_register_step(void) {
    if (!screen_register) create_register_screen(); // 确保屏幕已创建
    
    std::printf("[UI] Wizard Step 2: Face\n");

    // 1. 清理 Step 1 的控件
    lv_obj_clean(screen_register);
    lv_group_remove_all_objs(g_keypad_group);
    
    // 2. 标题
    lv_obj_t *label = lv_label_create(screen_register);
    lv_label_set_text(label, "Step 2: Capture Face");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    
    // 3. 摄像头预览区域 (复用全局 dsc)
    img_face_reg = lv_image_create(screen_register);
    lv_image_set_src(img_face_reg, &img_dsc); 
    lv_obj_set_size(img_face_reg, CAM_W, CAM_H);
    lv_obj_align(img_face_reg, LV_ALIGN_CENTER, 0, 0);
    
    // 给图片加个绿色边框，表示这是取景框
    lv_obj_set_style_border_width(img_face_reg, 3, 0);
    lv_obj_set_style_border_color(img_face_reg, lv_palette_main(LV_PALETTE_GREEN), 0);
    
    // 4. 将图片设为可交互，用来接收 ENTER 键
    lv_obj_add_flag(img_face_reg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img_face_reg, reg_step2_event_cb, LV_EVENT_KEY, nullptr);
    
    // 5. 提示
    lv_obj_t *tip = lv_label_create(screen_register);
    char tip_buf[128];
    std::snprintf(tip_buf, sizeof(tip_buf), "Hi, %s!\nPress ENTER to Capture", g_reg_name.c_str());
    lv_label_set_text(tip, tip_buf);
    lv_obj_set_style_text_color(tip, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // 6. 焦点设置
    lv_group_add_obj(g_keypad_group, img_face_reg);
    lv_group_focus_obj(img_face_reg);
    lv_screen_load(screen_register);
}

// ================= 初始化 =================

void ui_init(void) {
    lv_init();
    init_focus_style();
    lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
    (void)disp;
    
    lv_sdl_mouse_create();

    // 创建键盘设备
    lv_indev_t *kbd = lv_sdl_keyboard_create();
    
    if (kbd) {
        // 强制将键盘类型设为 Keypad (考勤机模式)
        // 这样 LVGL 才会把 方向键 当作 焦点切换键
        lv_indev_set_type(kbd, LV_INDEV_TYPE_KEYPAD);
        std::printf("[UI] Keyboard force set to KEYPAD mode.\n");
    }

    // 1. 确保创建了组
    g_keypad_group = lv_group_create();

    // 2. [重要] 开启循环模式 (Wrap)
    // 这样按到最后一个图标时，再按右键会自动跳回第一个
    lv_group_set_wrap(g_keypad_group, true);

    if (kbd) lv_indev_set_group(kbd, g_keypad_group);

    ui_theme_init();
    init_event_subscriptions();//订阅事件
    
    UiController::getInstance()->startBackgroundServices();// 通知 Controller 启动后台服务 (不再在 UI 层直接开线程)
    create_main_screen();

    load_main_screen();
    std::printf("[UI] Epic 4.4 Optimization: Threaded Capture Started.\n");
}

// =================  查询结果页 (Result Screen) =================

// 结果页通用事件回调
static void result_screen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // --- 焦点在结果容器 (查看内容) ---
        if (target == obj_result_container) {
            // ESC: 直接返回
            if (key == LV_KEY_ESC) {
                load_record_query_screen();
            }
            // 向右键: 切换焦点到“返回按钮” (因为上下键被用于滚动内容了)
            else if (key == LV_KEY_RIGHT) {
                if (btn_result_back) lv_group_focus_obj(btn_result_back);
            }
        }
        // --- 焦点在返回按钮 ---
        else if (target == btn_result_back) {
            if (key == LV_KEY_ENTER) {
                load_record_query_screen(); // 确认返回
            }
            else if (key == LV_KEY_LEFT) {
                // 向左键: 切回内容区继续查看
                if (obj_result_container) lv_group_focus_obj(obj_result_container);
            }
            else if (key == LV_KEY_ESC) {
                load_record_query_screen();
            }
        }
    }
    // 处理点击
    else if (code == LV_EVENT_CLICKED) {
        if (target == btn_result_back) {
            load_record_query_screen();
        }
    }
}

// 创建结果展示页 UI
static void create_record_result_screen(void) {
    if (screen_rec_result) return; 

    screen_rec_result = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_rec_result, lv_color_black(), 0);
    
    // 1. 内容容器 (高度减小，为底部按钮留出空间)
    // 原高度 320 -> 改为 260
    obj_result_container = lv_obj_create(screen_rec_result);
    lv_obj_set_size(obj_result_container, 230, 260); 
    lv_obj_align(obj_result_container, LV_ALIGN_TOP_MID, 0, 5); // 顶部对齐
    lv_obj_set_style_bg_color(obj_result_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(obj_result_container, 0, 0);
    lv_obj_set_flex_flow(obj_result_container, LV_FLEX_FLOW_COLUMN); 
    lv_obj_set_style_pad_all(obj_result_container, 5, 0);            
    lv_obj_set_scrollbar_mode(obj_result_container, LV_SCROLLBAR_MODE_AUTO);
    
    // 绑定事件 (用于处理 ESC 和 焦点切换)
    lv_obj_add_event_cb(obj_result_container, result_screen_event_cb, LV_EVENT_ALL, nullptr);

    // 2. [新增] 底部返回按钮
    btn_result_back = lv_button_create(screen_rec_result);
    lv_obj_set_size(btn_result_back, 120, 40);
    lv_obj_align(btn_result_back, LV_ALIGN_BOTTOM_MID, 0, -10); // 底部居中
    lv_obj_set_style_bg_color(btn_result_back, lv_color_hex(0x444444), 0);
    
    // 焦点样式 (红底黄框)
    lv_obj_add_style(btn_result_back, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_result_back, &style_btn_focused, LV_STATE_FOCUS_KEY);

    // 按钮文字
    lv_obj_t *lbl = lv_label_create(btn_result_back);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(lbl);
    
    // 绑定事件
    lv_obj_add_event_cb(btn_result_back, result_screen_event_cb, LV_EVENT_ALL, nullptr);
}

// 加载并显示指定员工的详细信息和记录
static void load_record_result_screen(int user_id) {
    if (!screen_rec_result) create_record_result_screen();
    
    // 1. 准备界面
    lv_obj_clean(obj_result_container);
    lv_group_remove_all_objs(g_keypad_group);
    
    // 2. [数据层交互] 获取员工详细档案
    UserData user = UiController::getInstance()->getUserInfo(user_id);
    
    // 3. 构建员工信息卡片
    lv_obj_t *header = lv_obj_create(obj_result_container);
    lv_obj_set_size(header, 210, 125); 
    lv_obj_set_style_bg_color(header, THEME_COLOR_BAR, 0);
    lv_obj_set_style_radius(header, 8, 0);
    lv_obj_set_style_pad_all(header, 10, 0);
    
    lv_obj_t *info_lbl = lv_label_create(header);
    lv_obj_add_style(info_lbl, &style_text_cn, 0);// 中文字体样式

    if (user.id == 0 && user.name.empty()) {
        lv_label_set_text(info_lbl, "User Not Found!\n查无此人");
        lv_obj_set_style_text_color(info_lbl, lv_palette_main(LV_PALETTE_RED), 0);
    } else {
        const char* role_str = (user.role == 1) ? "管理员" : "员工";
        lv_label_set_text_fmt(info_lbl, 
            "工号 ID: %d\n"
            "姓名 Name: %s\n"
            "部门 Dept: %d\n"
            "权限 Role: %s\n"
            "卡号 Card: %s", 
            user.id, 
            user.name.c_str(), 
            user.dept_id, 
            role_str, 
            user.card_id.c_str());
    }
    lv_obj_align(info_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    // 4. 构建记录列表标题
    lv_obj_t *title_rec = lv_label_create(obj_result_container);
    lv_label_set_text(title_rec, "--- 打卡记录 ---");
    lv_obj_set_style_text_color(title_rec, lv_color_hex(0xAAAAAA), 0);
    lv_obj_add_style(title_rec, &style_text_cn, 0);// 中文字体样式

    // 5. [数据层交互] 获取记录
    std::time_t now = std::time(nullptr);
    auto records = UiController::getInstance()->getRecords(-1, now - 30*24*3600, now + 24*3600); 
    
    bool found_any = false;
    for (const auto& rec : records) {
        if (rec.user_id == user_id) {
            found_any = true;
            
            lv_obj_t *rec_item = lv_obj_create(obj_result_container);
            lv_obj_set_size(rec_item, 210, 35);
            lv_obj_set_style_bg_color(rec_item, lv_color_hex(0x222222), 0);
            lv_obj_set_style_border_width(rec_item, 0, 0);
            lv_obj_clear_flag(rec_item, LV_OBJ_FLAG_SCROLLABLE);

            // 时间格式化修复
            char time_buf[32];
            time_t ts = (time_t)rec.timestamp; 
            struct tm *tm_info = localtime(&ts);
            strftime(time_buf, 32, "%m-%d %H:%M", tm_info);
            
            const char* status_icon = (rec.status == 0) ? LV_SYMBOL_OK : LV_SYMBOL_WARNING;
            lv_color_t status_color = (rec.status == 0) ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED);

            lv_obj_t *lbl = lv_label_create(rec_item);
            lv_label_set_text_fmt(lbl, "%s %s", status_icon, time_buf);
            lv_obj_set_style_text_color(lbl, status_color, 0);
            lv_obj_center(lbl);
        }
    }
    
    if (!found_any) {
        lv_obj_t *lbl = lv_label_create(obj_result_container);
        lv_label_set_text(lbl, "无记录 (No Data)");
        lv_obj_add_style(lbl, &style_text_cn, 0);// 中文字体样式
    }

    // 6. 切换屏幕并加入输入组
    lv_screen_load(screen_rec_result);
    
    // 【关键】将 容器 和 按钮 都加入组，实现焦点切换
    lv_group_add_obj(g_keypad_group, obj_result_container);
    lv_group_add_obj(g_keypad_group, btn_result_back);
    
    // 默认聚焦内容区，方便直接查看
    lv_group_focus_obj(obj_result_container);

    destroy_all_screens_except(screen_rec_result);//  清理其他屏幕资源
}
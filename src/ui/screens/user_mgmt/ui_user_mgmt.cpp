#include "ui_user_mgmt.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h" // 用于返回主菜单
#include "../../../business/event_bus.h"// 用于弹窗通知

#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

LV_FONT_DECLARE(font_noto_16);

namespace ui {
namespace user_mgmt {

// ================= [内部状态: 屏幕指针] =================
static lv_obj_t *scr_menu = nullptr;
static lv_obj_t *scr_list = nullptr;
static lv_obj_t *scr_camera = nullptr;
static lv_obj_t *scr_register = nullptr; 
static lv_obj_t *scr_info = nullptr;
static lv_obj_t *scr_del = nullptr;
static lv_obj_t *scr_pwd = nullptr;
static lv_obj_t *scr_role = nullptr;

// ================= [内部状态: 控件与数据] =================
static lv_obj_t *obj_list_view = nullptr;
static lv_obj_t *img_face_reg = nullptr;

static int g_reg_user_id = 0;
static std::string g_reg_name = "";
static int g_reg_dept_id = 0;

// ================= [前向声明] =================
static void force_nav_mode_timer_cb(lv_timer_t *t);
static void form_nav_event_cb(lv_event_t *e);
static void register_btn_next_event_handler(lv_event_t *e);
static void reg_step2_event_cb(lv_event_t *e);

//  用于异步关闭弹窗的上下文结构体
struct MsgBoxCloseCtx {
    lv_obj_t* mbox;
    lv_obj_t* restore_obj;
};

// ================= [实现部分] =================
static lv_image_dsc_t img_dsc_reg_cam = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB888,
        .flags = 0,
        .w = CAM_W,
        .h = CAM_H,
        .stride = CAM_W * 3, // 必须是 W * 3 (RGB888)
        .reserved_2 = 0
    },
    .data_size = CAM_W * CAM_H * 3,
    .data = nullptr, // 稍后在 create 函数中绑定 UiManager 的 Buffer
    .reserved = 0
};

// 辅助：统一显示操作结果弹窗
void user_init() {
    g_reg_user_id = 0;
    g_reg_name = "";
}

// =========================================================
// 1. 员工管理菜单 (Menu Screen)
// =========================================================

// 员工管理菜单按钮事件回调
static void user_menu_btn_event_cb(lv_event_t *e) {
    const char* tag = (const char*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    uint32_t key = 0;
    // 获取按键值 (如果不是按键事件，key 会是 0)
    if(code==LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 纯按键逻辑 (处理导航、退出、或者未来的数字快捷键)
    if (code == LV_EVENT_KEY) {

        if(key == LV_KEY_ESC) {
             ui::menu::load_menu_screen(); // 按 ESC 返回主页
             return; // 处理完返回后直接返回，避免继续执行下面的导航逻辑
         }
        // 导航
        if (key == LV_KEY_DOWN) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());// 向下导航
        } 
        else if (key == LV_KEY_UP) {
            lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());// 向上导航
        } 
    }
    
    // 2. 触发逻辑 (处理 回车键 和 触摸点击)
    // 注意：LVGL 会自动把 LV_KEY_ENTER 转换成 LV_EVENT_CLICKED，
    // 所以我们这里只需要处理 CLICKED，就能同时兼容 触摸屏 和 实体键盘回车。
    else if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {

        lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---

        if (tag == nullptr) return;// 安全检查

        // 根据按钮的 user_data（tag）来区分功能
        if (strcmp(tag, "LIST") == 0)      load_user_list_screen();// 员工列表
        else if (strcmp(tag, "REG") == 0)  load_user_register_form();// 员工注册
        else if (strcmp(tag, "DEL") == 0)  load_user_delete_screen();//删除员工
    }
}

// 主菜单界面实现
void load_user_menu_screen() {
    if (scr_menu){
        lv_obj_delete(scr_menu);
        scr_menu = nullptr;
    }

    BaseScreenParts parts = create_base_screen("员工管理");
    scr_menu = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_MGMT, &scr_menu);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_menu, [](lv_event_t * e) {
        scr_menu = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t * grid = create_menu_grid_container(parts.content);// 创建统一样式的菜单 Grid 容器

    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {70, 70, 70, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);// 设置 Grid 行列描述

    // 创建按钮
    create_sys_grid_btn(grid, 0, "1. ", "User List", "员工列表", user_menu_btn_event_cb, "LIST");
    create_sys_grid_btn(grid, 1, "2. ", "Register", "员工注册", user_menu_btn_event_cb, "REG");
    create_sys_grid_btn(grid, 2, "3. ", "Delete", "删除员工", user_menu_btn_event_cb, "DEL");
    
    UiManager::getInstance()->resetKeypadGroup();// 按键组管理

    uint32_t child_cnt = lv_obj_get_child_cnt(grid);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(grid, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(grid, 0));
    }

    lv_screen_load(scr_menu);
    UiManager::getInstance()->destroyAllScreensExcept(scr_menu);// 加载后销毁其他屏幕，保持资源清晰
}

// =========================================================
// 2. 员工列表 (List Screen)
// =========================================================

// 列表项事件回调
static void list_item_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    // 获取按键值 (如果不是按键事件，key 会是 0)
    if(code==LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 导航逻辑 (兼容键盘方向键)
    if (code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_user_menu_screen(); // 返回主菜单
            return;// 处理完返回后直接返回，避免继续执行下面的导航逻辑
         } 
        else if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());// 向下或向右导航
            return;// 处理完返回后直接返回，避免继续执行下面的导航逻辑
        } 
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());// 向上或向左导航
            return;
         }
    }
    
    // 2. 跳转详情页逻辑(逻辑：如果收到“点击” 或者 “按键是回车” -> 都视为触发)
    if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        
        lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---

        // 获取传递过来的 User ID
        // 注意：user_data 是在创建按钮时传入的 uid
        int uid = (int)(intptr_t)lv_event_get_user_data(e);
        
        load_user_info_screen(uid);// 跳转到员工详情页，传入 User ID
    }
}

// 员工列表界面实现
void load_user_list_screen() {
    if (scr_list){
        lv_obj_delete(scr_list);
        scr_list = nullptr;
    }

    BaseScreenParts parts = create_base_screen("员工列表");
    scr_list = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_LIST, &scr_list);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_list, [](lv_event_t * e) {
        scr_list = nullptr;
        obj_list_view = nullptr; // 把这个全局内容区指针也清空！
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // ==========================================
    // 将内容区改为 Flex 垂直布局，方便表头和列表堆叠
    // ==========================================
    lv_obj_set_flex_flow(parts.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parts.content, 5, 0);
    lv_obj_set_style_pad_gap(parts.content, 5, 0); // 表头和下方列表的间距

    // ==========================================
    // 创建独立表头行 (Header Row)
    // ==========================================
    lv_obj_t * header_row = lv_obj_create(parts.content);
    lv_obj_set_width(header_row, LV_PCT(100));
    lv_obj_set_height(header_row, 30);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width(header_row, 0, 0);       
    lv_obj_set_style_pad_all(header_row, 0, 0);

    // 开启横向排列，上下居中
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 表头 - 第 1 列：工号 (分配 25% 宽度，内部文字居中)
    lv_obj_t * lbl_h_id = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_id, LV_PCT(25));
    lv_label_set_text(lbl_h_id, "工号");
    lv_obj_set_style_text_color(lbl_h_id, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_id, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_id, LV_TEXT_ALIGN_CENTER, 0);

    // 表头 - 第 2 列：姓名 (分配 40% 宽度，内部文字居中)
    lv_obj_t * lbl_h_name = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_name, LV_PCT(40));
    lv_label_set_text(lbl_h_name, "姓名");
    lv_obj_set_style_text_color(lbl_h_name, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_name, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_name, LV_TEXT_ALIGN_CENTER, 0);

    // 表头 - 第 3 列：部门 (分配 35% 宽度，内部文字居中)
    lv_obj_t * lbl_h_dept = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_dept, LV_PCT(35));
    lv_label_set_text(lbl_h_dept, "部门");
    lv_obj_set_style_text_color(lbl_h_dept, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_dept, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_dept, LV_TEXT_ALIGN_CENTER, 0);

    // 创建列表容器 (挂在 parts.content 上)
    obj_list_view = lv_obj_create(parts.content);
    lv_obj_set_size(obj_list_view, LV_PCT(100), LV_PCT(100)); // 占满中心空白区
    lv_obj_set_flex_grow(obj_list_view, 1);// 让它占满剩余空间
    lv_obj_set_style_bg_opa(obj_list_view, LV_OPA_TRANSP, 0); // 让底层蓝色透过来
    lv_obj_set_style_border_width(obj_list_view, 0, 0);       // 去掉灰色边框
    lv_obj_set_style_pad_all(obj_list_view, 10, 0);           // 左右上下留一点呼吸空间
    lv_obj_set_style_pad_gap(obj_list_view, 8, 0);            // 列表项之间的间距
    lv_obj_set_flex_flow(obj_list_view, LV_FLEX_FLOW_COLUMN); // 开启垂直滚动的流式布局

    // 获取业务数据并动态生成列表项
    auto users = UiController::getInstance()->getAllUsers();
    
    if (users.empty()) {
        // 无数据时的缺省页显示
        lv_obj_t *lbl = lv_label_create(obj_list_view);
        lv_label_set_text(lbl, "暂无员工数据");
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_style_text_color(lbl, THEME_COLOR_TEXT_MAIN, 0);
        lv_obj_center(lbl); 
    } else {
        // 遍历生成用户按钮
        for (const auto& u : users) {
            lv_obj_t *btn = lv_button_create(obj_list_view); 
            lv_obj_set_width(btn, LV_PCT(100)); 
            lv_obj_set_height(btn, 45);         
            
            lv_obj_add_style(btn, &style_btn_default, 0);
            lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED); 
            
            // 去除按钮默认的内边距，并开启和表头完全一致的横向布局
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            
            std::string dname = u.dept_name.empty() ? "-" : u.dept_name;

            // 第 1 列：工号 (25% 宽度)
            lv_obj_t * l_id = lv_label_create(btn);
            lv_obj_set_width(l_id, LV_PCT(25));
            lv_label_set_long_mode(l_id, LV_LABEL_LONG_DOT); // 绝招：如果字太长，自动变成省略号(...)
            lv_label_set_text_fmt(l_id, "%d", u.id);
            lv_obj_add_style(l_id, &style_text_cn, 0);
            lv_obj_set_style_text_align(l_id, LV_TEXT_ALIGN_CENTER, 0);

            // 第 2 列：姓名 (40% 宽度)
            lv_obj_t * l_name = lv_label_create(btn);
            lv_obj_set_width(l_name, LV_PCT(40));
            lv_label_set_long_mode(l_name, LV_LABEL_LONG_DOT); // 自动省略号
            lv_label_set_text(l_name, u.name.c_str());
            lv_obj_add_style(l_name, &style_text_cn, 0);
            lv_obj_set_style_text_align(l_name, LV_TEXT_ALIGN_CENTER, 0);

            // 第 3 列：部门 (35% 宽度)
            lv_obj_t * l_dept = lv_label_create(btn);
            lv_obj_set_width(l_dept, LV_PCT(35));
            lv_label_set_long_mode(l_dept, LV_LABEL_LONG_DOT); // 自动省略号
            lv_label_set_text(l_dept, dname.c_str());
            lv_obj_add_style(l_dept, &style_text_cn, 0);
            lv_obj_set_style_text_align(l_dept, LV_TEXT_ALIGN_CENTER, 0);

            // 绑定事件和组
            lv_obj_set_user_data(btn, (void*)(intptr_t)u.id);
            lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_KEY, nullptr);
            UiManager::getInstance()->addObjToGroup(btn);
        }
        
        // 列表生成完后，默认聚焦第一项
        if (lv_obj_get_child_cnt(obj_list_view) > 0) {
            lv_group_focus_obj(lv_obj_get_child(obj_list_view, 0));
        }
    }
    
    // 兜底返回与屏幕加载
    // 处理在空白处的 ESC 兜底返回
    lv_obj_add_event_cb(scr_list, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) {
            lv_async_call([](void*){ load_user_menu_screen(); }, nullptr); // 防卡死异步调用
        }
    }, LV_EVENT_KEY, nullptr);

    // 加载这个全新生成的屏幕，并销毁其他老旧屏幕
    lv_screen_load(scr_list);
    UiManager::getInstance()->destroyAllScreensExcept(scr_list);
}

// =========================================================
// 3. 注册向导 (Registration Wizard) - 核心逻辑
// =========================================================

// 强制切回导航模式定时器回调
static void force_nav_mode_timer_cb(lv_timer_t * t) {
    if (UiManager::getInstance()->getKeypadGroup()) {
        lv_group_set_editing(UiManager::getInstance()->getKeypadGroup(), false);
    }
    lv_timer_del(t); 
}

// 表单控件导航事件回调
static void form_nav_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    auto group = UiManager::getInstance()->getKeypadGroup();

    if (code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_border_width(obj, 3, 0);

        if (group) {
            // 如果是下拉框或按钮，强制切回导航模式
            if (lv_obj_check_type(obj, &lv_dropdown_class) || 
                lv_obj_check_type(obj, &lv_button_class)) {
                lv_group_set_editing(group, false);
                lv_timer_create(force_nav_mode_timer_cb, 10, NULL); 
            }
            // 输入框保持编辑模式
            else if (lv_obj_check_type(obj, &lv_textarea_class)) {
                lv_group_set_editing(group, true);
            }
        }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_width(obj, 1, 0);
        lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        bool is_expanded = false;
        if (lv_obj_check_type(obj, &lv_dropdown_class)) is_expanded = lv_dropdown_is_open(obj);

        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            if (!is_expanded) {
                if (group) lv_group_set_editing(group, false);
                lv_group_focus_next(group);
            }
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            if (!is_expanded) {
                if (group) lv_group_set_editing(group, false);
                lv_group_focus_prev(group);
            }
        }
        else if (key == LV_KEY_ENTER) {
            if (lv_obj_check_type(obj, &lv_textarea_class)) {
                if (group) lv_group_set_editing(group, false);
                lv_group_focus_next(group);
            }
        }
    }
}

// ---  辅助函数：支持物理按键回车关闭 ---
static void show_modal_msg(const char* msg, lv_obj_t* restore_focus_obj) {
    // 1. 创建标准消息框
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "提示");
    lv_msgbox_add_text(mbox, msg);
    
    // 2. 添加按钮
    lv_obj_t * btn = lv_msgbox_add_footer_button(mbox, "我知道了");
    
    // 3. 将按钮加入按键组并强制聚焦
    lv_group_t * group = UiManager::getInstance()->getKeypadGroup();
    lv_group_add_obj(group, btn); 
    lv_group_focus_obj(btn);     

    // 4. 处理按钮事件
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        bool should_close = false;

        // A. 触摸屏点击
        if (code == LV_EVENT_CLICKED) {
            should_close = true;
        }
        // B. 物理按键 (回车 或 Home键)
        else if (code == LV_EVENT_KEY) {
            uint32_t key = lv_event_get_key(e);
            if (key == LV_KEY_ENTER || key == LV_KEY_HOME) {
                should_close = true;
            }
        }

        if (should_close) {
            lv_obj_t* btn_obj = (lv_obj_t*)lv_event_get_target(e);
            
            // 1. 暂时禁用按钮，防止多次触发
            lv_obj_clear_flag(btn_obj, LV_OBJ_FLAG_CLICKABLE);

            // 2. 准备异步数据
            // 获取 mbox (按钮的父对象的父对象)
            lv_obj_t* mbox_obj = lv_obj_get_parent(lv_obj_get_parent(btn_obj));
            // 获取要恢复焦点的对象
            lv_obj_t* restore = (lv_obj_t*)lv_event_get_user_data(e);
            
            MsgBoxCloseCtx* ctx = new MsgBoxCloseCtx{mbox_obj, restore};

            // 3. 异步延迟关闭！
            // 等待当前的按键事件彻底结束，下一帧再关闭弹窗并恢复焦点
            lv_async_call([](void* user_data){
                MsgBoxCloseCtx* c = (MsgBoxCloseCtx*)user_data;
                
                // 关闭弹窗
                if (c->mbox) lv_msgbox_close(c->mbox);
                
                // 恢复焦点 (此时回车键事件已经过去了，不会再误触)
                if (c->restore_obj && lv_obj_is_valid(c->restore_obj)) {
                    lv_group_focus_obj(c->restore_obj);
                }
                
                delete c;
            }, ctx);
        }
    }, LV_EVENT_ALL, restore_focus_obj);
}

// 注册 Step 1: 加载表单
void load_user_register_form() {
    if (scr_register){
        lv_obj_delete(scr_register);
        scr_register = nullptr;
    }

    int next_user_id = UiController::getInstance()->generateNextUserId();
    BaseScreenParts parts = create_base_screen("员工注册");
    scr_register = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::REGISTER, &scr_register);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_register, [](lv_event_t * e) {
        scr_register = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 设置内容区为垂直 Flex 布局，方便后续堆叠表单行和按钮
    lv_obj_set_flex_flow(parts.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parts.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parts.content, 12, 0);

    // --- 通用 ESC ---
    auto form_esc_cb = [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
            lv_async_call([](void*) { load_user_menu_screen(); }, nullptr);
        }
    };

    // --- 辅助函数 ---
    auto create_text_row = [&](const char* label_txt, const char* default_text, bool is_readonly) -> lv_obj_t* {
        lv_obj_t* row = lv_obj_create(parts.content);
        lv_obj_set_size(row, LV_PCT(95), 50);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(row, 0, 0);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label_txt);
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_width(lbl, 60);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0); // 白色标签

        lv_obj_t* ta = lv_textarea_create(row);
        lv_textarea_set_one_line(ta, true);
        lv_obj_set_flex_grow(ta, 1);
        lv_obj_add_event_cb(ta, form_esc_cb, LV_EVENT_KEY, nullptr);

        if (is_readonly) {
            lv_textarea_set_text(ta, default_text);
            lv_obj_remove_flag(ta, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(ta, LV_OBJ_FLAG_SCROLLABLE); 
            lv_textarea_set_cursor_click_pos(ta, false);
            // 方案1：高亮白底风格 (最清晰)
            lv_obj_set_style_bg_color(ta, lv_color_hex(0xF5F5F5), 0); 
            lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(ta, lv_color_black(), 0);
        } else {
            lv_textarea_set_placeholder_text(ta, default_text);
        }
        return ta;
    };

    // 4. 创建控件
    // [工号]
    std::string id_str = std::to_string(next_user_id);
    lv_obj_t* ta_id = create_text_row("工号:", id_str.c_str(), true); 

    // [姓名]
    lv_obj_t* ta_name = create_text_row("姓名:", "请输入姓名:name", false);
    
    // [部门] (创建下拉框行)
    lv_obj_t* row_dept = lv_obj_create(parts.content);
    lv_obj_set_size(row_dept, LV_PCT(95), 50);
    lv_obj_set_flex_flow(row_dept, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_dept, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_dept, 0, 0);
    lv_obj_set_style_bg_opa(row_dept, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(row_dept, 0, 0);

    lv_obj_t* lbl_dept = lv_label_create(row_dept);
    lv_label_set_text(lbl_dept, "部门:");
    lv_obj_add_style(lbl_dept, &style_text_cn, 0);
    lv_obj_set_width(lbl_dept, 60);
    lv_obj_set_style_text_align(lbl_dept, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(lbl_dept, lv_color_white(), 0);

    lv_obj_t* dd_dept = lv_dropdown_create(row_dept);
    lv_obj_set_flex_grow(dd_dept, 1);
    lv_obj_remove_flag(dd_dept, LV_OBJ_FLAG_SCROLLABLE);
    
    auto depts = UiController::getInstance()->getDepartmentList();
    std::string opts = "";
    for (const auto& d : depts) {
        if (!opts.empty()) opts += "\n";
        opts += d.name;
    }
    if (opts.empty()) opts = "Default";
    lv_dropdown_set_options(dd_dept, opts.c_str());
    lv_obj_add_style(dd_dept, &style_text_cn, 0);
    lv_obj_add_event_cb(dd_dept, form_esc_cb, LV_EVENT_KEY, nullptr);
    
    // 列表样式
    lv_obj_t* list = lv_dropdown_get_list(dd_dept);
    if (list) lv_obj_add_style(list, &style_text_cn, 0);

    // [注册按钮]
    lv_obj_t* btn_next = lv_button_create(parts.content);
    lv_obj_set_size(btn_next, LV_PCT(50), 40);
    lv_obj_add_style(btn_next, &style_btn_default, 0);
    lv_obj_add_style(btn_next, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_next, form_esc_cb, LV_EVENT_KEY, nullptr);

    lv_obj_t* lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, "注册");
    lv_obj_add_style(lbl_next, &style_text_cn, 0);
    lv_obj_center(lbl_next);

    // ============================================================
    // 键盘流逻辑 (Chain Logic)
    // ============================================================
    
    // 定义一个结构体来传递这几个关键控件的指针
    struct ChainContext {
        lv_obj_t* ta_name;
        lv_obj_t* dd_dept;
        lv_obj_t* btn_next;
    };
    ChainContext* chain = new ChainContext{ta_name, dd_dept, btn_next};

    // 1. 【姓名框逻辑】：按下 Enter 或 下箭头 -> 跳转部门并展开
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e) {
        ChainContext* c = (ChainContext*)lv_event_get_user_data(e);
        lv_event_code_t code = lv_event_get_code(e);

        // 1. 判断按键类型
        bool is_enter = (code == LV_EVENT_READY) || 
                        (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER);
        
        bool is_down = (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_DOWN);

        // 2. 处理 [向下键] 的特殊逻辑：必须校验非空
        if (is_down) {
            const char* txt = lv_textarea_get_text(c->ta_name);
            if (strlen(txt) == 0) {
                // 如果为空，异步弹出提示，并阻止跳转
                lv_async_call([](void* user_data){
                    lv_obj_t* ta = (lv_obj_t*)user_data;
                    lv_group_focus_obj(ta); // 确保焦点在输入框
                    show_modal_msg("姓名不能为空！", ta); 
                }, c->ta_name);
                return; // 直接返回，不执行后面的跳转
            }
        }

        // 3. 执行跳转 (Enter 直接跳，Down 只有通过校验才会走到这里)
        if (is_enter || is_down) {
            lv_async_call([](void* user_data){
                lv_obj_t* dd = (lv_obj_t*)user_data;
                lv_group_focus_obj(dd); // 焦点移到部门
                lv_dropdown_open(dd);   // 展开下拉框
            }, c->dd_dept);
        }

    }, LV_EVENT_ALL, chain);

    // 2. 【部门框逻辑】：按下 Enter -> 选中并跳转按钮
    // 注意：LVGL下拉框打开时，按键由列表处理，但确认选择(Enter)后，
    // 事件会冒泡回 Dropdown 或者焦点会回到 Dropdown。
    lv_obj_add_event_cb(dd_dept, [](lv_event_t* e) {
        ChainContext* c = (ChainContext*)lv_event_get_user_data(e);
        // 当按下回车确认选择时
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            // 同样使用 async_call 延时跳到按钮
            lv_async_call([](void* user_data){
                lv_obj_t* btn = (lv_obj_t*)user_data;
                lv_group_focus_obj(btn);
            }, c->btn_next);
        }
    }, LV_EVENT_KEY, chain);

    // ============================================================

    // 5. 注册逻辑 (按钮点击)
    lv_obj_t** widgets = new lv_obj_t*[3];
    widgets[0] = ta_id; widgets[1] = ta_name; widgets[2] = dd_dept;

    auto next_step_cb = [](lv_event_t* e) {
        // A. 获取触发事件的类型
        lv_event_code_t code = lv_event_get_code(e);
        
        bool is_triggered = false;

        // B. 判断触发条件
        // 条件1: 触摸屏点击
        if (code == LV_EVENT_CLICKED) {
            is_triggered = true;
        }
        // 条件2: 物理按键 (回车 或 Home键/确认键)
        else if (code == LV_EVENT_KEY) {
            uint32_t key = lv_event_get_key(e);
            if (key == LV_KEY_ENTER || key == LV_KEY_HOME) {
                is_triggered = true;
            }
        }

        // C. 如果触发了，执行业务逻辑
        if (is_triggered) {
            lv_obj_t** w_arr = (lv_obj_t**)lv_event_get_user_data(e);
            
            const char* txt_id = lv_textarea_get_text(w_arr[0]);
            const char* txt_name = lv_textarea_get_text(w_arr[1]);
            
            // 非空校验
            if (strlen(txt_name) == 0) {
                show_modal_msg("姓名不能为空，请填写！", w_arr[1]);
                return;
            }

            // 获取部门 ID
            lv_obj_t* dd = w_arr[2];
            uint32_t selected_idx = lv_dropdown_get_selected(dd);
            auto depts_list = UiController::getInstance()->getDepartmentList();
            int selected_dept_id = (!depts_list.empty() && selected_idx < depts_list.size()) 
                                   ? depts_list[selected_idx].id : 0;

            g_reg_user_id = std::atoi(txt_id);
            g_reg_name = txt_name;
            g_reg_dept_id = selected_dept_id;

            printf("[UI] Reg Step 1: ID=%d, Name=%s, DeptID=%d\n", g_reg_user_id, g_reg_name.c_str(), g_reg_dept_id);
            lv_async_call([](void*) { load_user_register_camera_step(); }, nullptr);
        }
    };

    lv_obj_add_event_cb(btn_next, next_step_cb, LV_EVENT_ALL, widgets);
    
    // 内存清理 (清理 widgets 数组 和 chain 结构体)
    lv_obj_add_event_cb(btn_next, [](lv_event_t* e){
        delete[] (lv_obj_t**)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, widgets); // 这里清理 widgets

    // 把 ChainContext 的清理挂在 ta_name 的删除事件上 (或者 btn_next 上也可以，只要保证不漏)
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e){
        delete (ChainContext*)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, chain);

    // 物理按键兼容
    lv_obj_add_event_cb(btn_next, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ENTER || lv_event_get_key(e) == LV_KEY_HOME) {
             lv_obj_send_event((lv_obj_t*)lv_event_get_target(e), LV_EVENT_CLICKED, lv_event_get_user_data(e));
        }
    }, LV_EVENT_KEY, widgets);

    // 6. 加入按键组
    lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
    lv_group_remove_all_objs(group);
    
    lv_group_add_obj(group, ta_name);
    lv_group_add_obj(group, dd_dept);
    lv_group_add_obj(group, btn_next);
    
    lv_group_focus_obj(ta_name); // 默认聚焦姓名，准备开始输入！

    lv_screen_load(scr_register);

    UiManager::getInstance()->destroyAllScreensExcept(scr_register);// 这里我们已经在注册界面了，理论上不应该有其他界面了，但为了保险起见，还是调用一下销毁其他屏幕的函数，防止内存泄漏。

}

// 注册按钮事件回调
static void register_btn_next_event_handler(lv_event_t * e) {
    lv_obj_t * name_ta = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t * dept_dd = (lv_obj_t *)lv_obj_get_user_data(name_ta);

    const char * name_txt = lv_textarea_get_text(name_ta);
    uint16_t selected_dept_idx = lv_dropdown_get_selected(dept_dd);

    if (strlen(name_txt) == 0) {
        lv_obj_set_style_border_color(name_ta, lv_palette_main(LV_PALETTE_RED), 0);
        return;
    }

    auto depts = UiController::getInstance()->getDepartmentList();
    if (selected_dept_idx < depts.size()) {
        g_reg_dept_id = depts[selected_dept_idx].id;
    } else {
        g_reg_dept_id = 0; 
    }
    g_reg_name = std::string(name_txt);

    load_user_register_camera_step();
}

// 注册 Step 2: 加载拍照界面
void load_user_register_camera_step() {
    if (scr_camera){
        lv_obj_delete(scr_camera);
        scr_camera = nullptr;
    }

    BaseScreenParts parts = create_base_screen("注册拍照");
    scr_camera = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::REGISTER_CAMERA, &scr_camera);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_camera, [](lv_event_t * e) {
        scr_camera = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件
    
    // 准备摄像头数据显示 
    static lv_image_dsc_t img_dsc;
    img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
    img_dsc.header.w = CAM_W;
    img_dsc.header.h = CAM_H;
    img_dsc.header.stride = CAM_W * 3;
    img_dsc.data_size = CAM_W * CAM_H * 3;
    img_dsc.data = UiManager::getInstance()->getCameraDisplayBuffer(); // 获取共享内存

    // 创建显示摄像头的图片控件
    img_face_reg = lv_image_create(parts.content);
    lv_image_set_src(img_face_reg, &img_dsc);
    lv_obj_set_size(img_face_reg, 240, 210);
    lv_obj_align(img_face_reg, LV_ALIGN_CENTER, 0, -20);
    
    // 样式美化 (绿色边框)
    lv_obj_set_style_border_width(img_face_reg, 3, 0);
    lv_obj_set_style_border_color(img_face_reg, lv_palette_main(LV_PALETTE_GREEN), 0);
 
    // 添加提示文字
    lv_obj_t* lbl_hint = lv_label_create(parts.content);
    lv_label_set_text_fmt(lbl_hint, "Hi, %s!\nPress ENTER to Register", g_reg_name.c_str());
    lv_obj_set_style_text_align(lbl_hint, LV_TEXT_ALIGN_CENTER, 0);// 居中对齐
    lv_obj_set_style_text_color(lbl_hint, lv_color_white(), 0);
    lv_obj_align_to(lbl_hint, img_face_reg, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    //绑定按键事件
    lv_obj_add_event_cb(img_face_reg, [](lv_event_t* e){
        uint32_t key = lv_event_get_key(e);

        // --- 情况 A: 按下回车 (注册) ---
        if (key == LV_KEY_ENTER) {
            // 直接调用注册接口
            bool success = UiController::getInstance()->registerNewUser(g_reg_name, g_reg_dept_id);

            if (success) {
                // 成功提示
                show_modal_msg("Success\nUser Registered!", nullptr);
                
                // 1.5秒后返回菜单
                lv_timer_create([](lv_timer_t* t){
                    load_user_menu_screen();
                    lv_timer_del(t);
                }, 1500, nullptr);
            } else {
                // 失败提示
                show_modal_msg("Error\nRegistration Failed!", nullptr);
            }
        }
        // --- 情况 B: 按下 ESC (返回) ---
        else if (key == LV_KEY_ESC) {
            img_face_reg = nullptr; // 清空指针防止野指针
            load_user_register_form(); // 返回填表界面
        }

    }, LV_EVENT_KEY, nullptr);

    // 焦点设置
    // 这一步必须做，否则接收不到键盘事件
    lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
    lv_group_remove_all_objs(group);
    lv_group_add_obj(group, img_face_reg);
    lv_group_focus_obj(img_face_reg);

    // 订阅摄像头刷新事件，让画面动起来
    EventBus::getInstance().subscribe(EventType::CAMERA_FRAME_READY, [](void* data) {
       lv_async_call([](void* d) {
           if (img_face_reg && lv_obj_is_valid(img_face_reg)) {
               lv_obj_invalidate(img_face_reg);
           }
       }, nullptr);
    });

    lv_screen_load(scr_camera);

    UiManager::getInstance()->destroyAllScreensExcept(scr_camera);

}

// 注册 Step 2: 拍照事件回调
static void reg_step2_event_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ENTER) {
        if (UiController::getInstance()->registerNewUser(g_reg_name.c_str(), g_reg_dept_id)) {
            show_popup("Success", "User Registered!");
            lv_timer_t *t = lv_timer_create([](lv_timer_t*){ load_user_menu_screen(); }, 1500, nullptr);
            lv_timer_set_repeat_count(t, 1);
        } else {
            show_popup("Error", "Registration Failed!\n(Check DB/Dept ID)");
        }
    } else if (lv_event_get_key(e) == LV_KEY_ESC) {
        img_face_reg = nullptr;
        load_user_register_form(); 
    }
}

// =========================================================
// 4. 删除用户 (Delete Screen)
// =========================================================

void load_user_delete_screen() {
    if(scr_del){
        lv_obj_delete(scr_del);
        scr_del = nullptr;
    }

    BaseScreenParts parts = create_base_screen("删除用户");
    scr_del = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DELETE_USER, &scr_del);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_del, [](lv_event_t * e) {
        scr_del = nullptr;
    }, LV_EVENT_DELETE, NULL);
    
    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 输入框
    lv_obj_t *ta_id = lv_textarea_create(scr_del);
    lv_textarea_set_one_line(ta_id, true);
    lv_textarea_set_placeholder_text(ta_id, "输入工号 (ID)");
    lv_textarea_set_accepted_chars(ta_id, "0123456789"); 
    lv_textarea_set_max_length(ta_id, 8);
    lv_obj_set_width(ta_id, LV_PCT(80));
    lv_obj_align(ta_id, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_style(ta_id, &style_btn_focused, LV_STATE_FOCUSED);

    // 删除按钮
    lv_obj_t *btn_del = lv_button_create(scr_del);
    lv_obj_set_width(btn_del, LV_PCT(60));
    lv_obj_align(btn_del, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_style(btn_del, &style_btn_focused, LV_STATE_FOCUSED);
    
    lv_obj_t *lbl_btn = lv_label_create(btn_del);
    lv_label_set_text(lbl_btn, "确认删除");
    lv_obj_add_style(lbl_btn, &style_text_cn, 0);
    lv_obj_center(lbl_btn);

    // 删除事件逻辑
    lv_obj_add_event_cb(btn_del, [](lv_event_t *e) {
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
            const char *txt = lv_textarea_get_text(ta);
            if (strlen(txt) > 0) {
                int uid = atoi(txt);
                // 调用业务层删除
                if (UiController::getInstance()->deleteUser(uid)) {
                    show_popup("Success", "User Deleted");
                    // 延时返回菜单
                    lv_timer_t *t = lv_timer_create([](lv_timer_t*){ load_user_menu_screen(); }, 1500, nullptr);
                    lv_timer_set_repeat_count(t, 1);
                } else {
                    show_popup("Error", "ID Not Found");
                    lv_textarea_set_text(ta, ""); // 清空重输
                }
            }
        }
    }, LV_EVENT_KEY, ta_id);

    // ESC 返回
    lv_obj_add_event_cb(ta_id, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_user_menu_screen();
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(ta_id);
    UiManager::getInstance()->addObjToGroup(btn_del);
    lv_group_focus_obj(ta_id);

    lv_screen_load(scr_del);
    UiManager::getInstance()->destroyAllScreensExcept(scr_del);
}

// =========================================================
// 5. 员工详情页 (User Info) 
// =========================================================

void load_user_info_screen(int user_id) {
    if (scr_info) {
        lv_obj_delete(scr_info);
        scr_info = nullptr;
    }

    BaseScreenParts parts = create_base_screen("员工详情");
    scr_info = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_INFO, &scr_info);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_info, [](lv_event_t * e) {
        scr_info = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    UserData u = UiController::getInstance()->getUserInfo(user_id);// 从业务层获取用户数据

    // 4. 创建内容容器 (使用 Grid 布局来排列各项信息)
    lv_obj_t *grid = lv_obj_create(parts.content);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(grid, 0, 0);       // 透明背景
    lv_obj_set_style_border_width(grid, 0, 0); // 无边框
    lv_obj_set_style_pad_all(grid, 0, 0);      // 无内边距

    // 定义 Grid 行列 (2列 x 8行)
    // 第一列宽度固定 90px (放标签)，第二列占满剩余空间 (1fr)
    static int32_t col_dsc[] = {90, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {45, 45, 45, 45, 45, 45, 45, 45, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    // 5. 重置按键组 (准备添加新控件)
    UiManager::getInstance()->resetKeypadGroup();
    lv_group_t* group = UiManager::getInstance()->getKeypadGroup();

    // --- 辅助 Lambda: 快速创建一行详情 ---
    auto create_row = [&](int row, const char* label, lv_obj_t* content) {
        // A. 左侧标签
        lv_obj_t *lbl = lv_label_create(grid);
        lv_label_set_text(lbl, label);
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_grid_cell(lbl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
        
        // B. 右侧内容 (如果存在)
        if (content) {
            lv_obj_set_grid_cell(content, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, row, 1);
            
            // 如果是可交互对象 (输入框/按钮)，加入按键组并处理 ESC
            if (lv_obj_has_flag(content, LV_OBJ_FLAG_CLICKABLE) || lv_obj_check_type(content, &lv_textarea_class)) {
                lv_obj_add_style(content, &style_btn_focused, LV_STATE_FOCUSED);
                lv_group_add_obj(group, content); 
                
                // 绑定 ESC 返回列表 (防止焦点陷阱)
                lv_obj_add_event_cb(content, [](lv_event_t* e){
                    if (lv_event_get_key(e) == LV_KEY_ESC) {
                        load_user_list_screen();
                    }
                }, LV_EVENT_KEY, nullptr);
            }
        }
        return content;
    };

    // --- [Row 0] 工号 (只读) ---
    lv_obj_t *lbl_id = lv_label_create(grid);
    lv_label_set_text_fmt(lbl_id, "%d", u.id);
    lv_obj_set_style_text_color(lbl_id, lv_color_white(), 0);
    lv_obj_align(lbl_id, LV_ALIGN_LEFT_MID, 0, 0); 
    create_row(0, "工号", lbl_id);

    // --- [Row 1] 姓名 (可编辑) ---
    lv_obj_t *ta_name = lv_textarea_create(grid);
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_text(ta_name, u.name.c_str());
    lv_obj_set_user_data(ta_name, (void*)(intptr_t)u.id);
    // 逻辑：失去焦点时自动保存修改
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e){
         if(lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
             lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
             int uid = (int)(intptr_t)lv_event_get_user_data(e);
             UiController::getInstance()->updateUserName(uid, lv_textarea_get_text(ta));
         }
    }, LV_EVENT_ALL, nullptr);
    create_row(1, "姓名", ta_name);

    // --- [Row 2] 人脸 (按钮) ---
    lv_obj_t *btn_face = lv_button_create(grid);
    lv_obj_t *lbl_face = lv_label_create(btn_face);
    bool has_face = !u.face_feature.empty();
    lv_label_set_text(lbl_face, has_face ? "已注册 (重录)" : "未注册 (录入)");
    lv_obj_add_style(lbl_face, &style_text_cn, 0);
    lv_obj_center(lbl_face);
    lv_obj_set_user_data(btn_face, (void*)(intptr_t)u.id);
    // 逻辑：点击/回车 -> 跳转拍照页
    lv_obj_add_event_cb(btn_face, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {
            int uid = (int)(intptr_t)lv_event_get_user_data(e);
            
            // 设置全局注册变量，供拍照页使用
            g_reg_user_id = uid; 
            g_reg_name = UiController::getInstance()->getUserInfo(uid).name; 
            g_reg_dept_id = UiController::getInstance()->getUserInfo(uid).dept_id;

            load_user_register_camera_step(); // 跳转去拍照
        }
    }, LV_EVENT_ALL, nullptr);
    create_row(2, "人脸", btn_face);

    // --- [Row 3] 部门 (只读) ---
    lv_obj_t *lbl_dept = lv_label_create(grid);
    lv_label_set_text(lbl_dept, u.dept_name.c_str());
    lv_obj_add_style(lbl_dept, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_dept, lv_color_white(), 0);
    create_row(3, "部门", lbl_dept);

    // --- [Row 4] 指纹 (占位) ---
    lv_obj_t *lbl_fp = lv_label_create(grid);
    lv_label_set_text(lbl_fp, "暂不支持");
    lv_obj_add_style(lbl_fp, &style_text_cn, 0);
    create_row(4, "指纹", lbl_fp);

    // --- [Row 5] 密码 (按钮) ---
    lv_obj_t *btn_pwd = lv_button_create(grid);
    lv_obj_t *lbl_pwd = lv_label_create(btn_pwd);
    bool has_pwd = !u.password.empty();
    lv_label_set_text(lbl_pwd, has_pwd ? "已设置 (修改)" : "未设置 (添加)");
    lv_obj_add_style(lbl_pwd, &style_text_cn, 0);
    lv_obj_center(lbl_pwd);
    lv_obj_set_user_data(btn_pwd, (void*)(intptr_t)u.id);
    // 逻辑：跳转修改密码页
    lv_obj_add_event_cb(btn_pwd, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {
            load_user_password_change_screen((int)(intptr_t)lv_event_get_user_data(e));
        }
    }, LV_EVENT_ALL, nullptr);
    create_row(5, "密码", btn_pwd);

    // --- [Row 6] 卡号 (只读) ---
    lv_obj_t *lbl_card = lv_label_create(grid);
    lv_label_set_text(lbl_card, u.card_id.empty() ? "无" : u.card_id.c_str());
    lv_obj_set_style_text_color(lbl_card, lv_color_white(), 0);
    create_row(6, "卡号", lbl_card);

    // --- [Row 7] 权限 (按钮) ---
    lv_obj_t *btn_role = lv_button_create(grid);
    lv_obj_t *lbl_role = lv_label_create(btn_role);
    lv_label_set_text(lbl_role, (u.role == 1) ? "管理员" : "普通员工");
    lv_obj_add_style(lbl_role, &style_text_cn, 0);
    lv_obj_center(lbl_role);
    lv_obj_set_user_data(btn_role, (void*)(intptr_t)u.id);
    // 逻辑：跳转修改权限页
    lv_obj_add_event_cb(btn_role, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {
            int uid = (int)(intptr_t)lv_event_get_user_data(e);
            int r = UiController::getInstance()->getUserInfo(uid).role;
            load_user_role_change_screen(uid, r);
        }
    }, LV_EVENT_ALL, nullptr);
    create_row(7, "权限", btn_role);

    // 默认聚焦：姓名输入框
    lv_group_focus_obj(ta_name);

    // 6. 全局 ESC 返回 (防止有漏网之鱼)
    lv_obj_add_event_cb(scr_info, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ESC) load_user_list_screen();
    }, LV_EVENT_KEY, nullptr);

    lv_screen_load(scr_info);
    UiManager::getInstance()->destroyAllScreensExcept(scr_info);
}

// =========================================================
// 6. 修改密码页 (Password Change) - 完整双重校验逻辑
// =========================================================

void load_user_password_change_screen(int user_id) {
    if (scr_pwd) {
        lv_obj_delete(scr_pwd);
        scr_pwd = nullptr;
    }

    BaseScreenParts parts = create_base_screen("密码设置");
    scr_pwd = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::PWD_CHANGE, &scr_pwd);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_pwd, [](lv_event_t * e) {
        scr_pwd = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 容器
    lv_obj_t *cont = lv_obj_create(scr_pwd);
    lv_obj_set_size(cont, 220, 200);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);

    // 输入框 1: 新密码
    lv_obj_t *p1 = lv_textarea_create(cont);
    lv_textarea_set_password_mode(p1, true);
    lv_textarea_set_placeholder_text(p1, "输入新密码");
    lv_textarea_set_one_line(p1, true);
    lv_obj_set_width(p1, LV_PCT(100));
    lv_obj_add_style(p1, &style_btn_focused, LV_STATE_FOCUSED);

    // 输入框 2: 确认密码
    lv_obj_t *p2 = lv_textarea_create(cont);
    lv_textarea_set_password_mode(p2, true);
    lv_textarea_set_placeholder_text(p2, "再次输入");
    lv_textarea_set_one_line(p2, true);
    lv_obj_set_width(p2, LV_PCT(100));
    lv_obj_add_style(p2, &style_btn_focused, LV_STATE_FOCUSED);

    // 按钮区
    lv_obj_t *btn_box = lv_obj_create(cont);
    lv_obj_set_size(btn_box, LV_PCT(100), 50);
    lv_obj_set_flex_flow(btn_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_box, LV_OPA_TRANSP, 0);

    // 确认按钮
    lv_obj_t *btn_ok = lv_button_create(btn_box);
    lv_label_set_text(lv_label_create(btn_ok), "确认");
    lv_obj_add_style(btn_ok, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(lv_obj_get_child(btn_ok, 0), &style_text_cn, 0);

    // 上下文传递结构体
    struct Ctx { int uid; lv_obj_t *t1; lv_obj_t *t2; };
    Ctx *ctx = new Ctx{user_id, p1, p2}; 

    // 绑定删除回调防止内存泄漏
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        Ctx* c = (Ctx*)lv_event_get_user_data(e);
        if (c) delete c; 
    }, LV_EVENT_DELETE, NULL);

    // 确认逻辑
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            Ctx* c = (Ctx*)lv_event_get_user_data(e);
            const char* s1 = lv_textarea_get_text(c->t1);
            const char* s2 = lv_textarea_get_text(c->t2);
            
            if (strlen(s1) > 0 && strcmp(s1, s2) == 0) {
                UiController::getInstance()->updateUserPassword(c->uid, s1);
                show_popup("Success", "Password Updated");
                load_user_info_screen(c->uid); // 成功后返回详情
            } else {
                show_popup("Error", "Password Mismatch\nor Empty");
            }
        }
    }, LV_EVENT_KEY, ctx);
    lv_obj_set_user_data(btn_ok, ctx);

    // 取消按钮
    lv_obj_t *btn_cancel = lv_button_create(btn_box);
    lv_label_set_text(lv_label_create(btn_cancel), "取消");
    lv_obj_add_style(btn_cancel, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(lv_obj_get_child(btn_cancel, 0), &style_text_cn, 0);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e){
         if (lv_event_get_key(e) == LV_KEY_ENTER) {
             // 清空输入
             Ctx* c = (Ctx*)lv_event_get_user_data(e); // 复用同一个Ctx注意生命周期，这里简单处理
             lv_textarea_set_text(c->t1, "");
             lv_textarea_set_text(c->t2, "");
             // 实际上应该返回上一级
             load_user_info_screen(c->uid);
         }
    }, LV_EVENT_KEY, ctx);

    // ESC 绑定到输入框
    lv_obj_add_event_cb(p1, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ESC) 
             load_user_info_screen((int)(intptr_t)lv_event_get_user_data(e));
    }, LV_EVENT_KEY, (void*)(intptr_t)user_id);

    // 输入组
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(p1);
    UiManager::getInstance()->addObjToGroup(p2);
    UiManager::getInstance()->addObjToGroup(btn_ok);
    UiManager::getInstance()->addObjToGroup(btn_cancel);
    lv_group_focus_obj(p1);

    lv_screen_load(scr_pwd);
    UiManager::getInstance()->destroyAllScreensExcept(scr_pwd);
}

// =========================================================
// 7. 权限变更页 (Role Change)
// =========================================================

void load_user_role_change_screen(int user_id, int current_role) {
    if (scr_role) {
        lv_obj_delete(scr_role);
        scr_role = nullptr;
    }

    BaseScreenParts parts = create_base_screen("设置权限");
    scr_role = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::ROLE_AUTH, &scr_role);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_role, [](lv_event_t * e) {
        scr_role = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 输入框：验证当前用户密码
    lv_obj_t *ta = lv_textarea_create(scr_role);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "Admin Password");
    lv_textarea_set_one_line(ta, true);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(ta, &style_btn_focused, LV_STATE_FOCUSED);

    struct Ctx { int uid; int role; };
    Ctx* ctx = new Ctx{user_id, current_role}; 

    // 内存清理
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        delete (Ctx*)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, nullptr);

    // 验证逻辑
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            Ctx* c = (Ctx*)lv_event_get_user_data(e);
            const char* pwd = lv_textarea_get_text((lv_obj_t*)lv_event_get_target(e));
            // 简单硬编码校验
            if(strcmp(pwd, "123456") == 0) { 
                UiController::getInstance()->updateUserRole(c->uid, c->role == 0 ? 1 : 0);
                show_popup("Success", "Role Updated");
                load_user_info_screen(c->uid);
            } else {
                show_popup("Error", "Wrong Password");
                lv_textarea_set_text((lv_obj_t*)lv_event_get_target(e), "");
            }
        }
    }, LV_EVENT_KEY, ctx);

    // ESC
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
         if(lv_event_get_key(e) == LV_KEY_ESC) {
             Ctx* c = (Ctx*)lv_event_get_user_data(e);
             load_user_info_screen(c->uid);
         }
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(ta);
    lv_group_focus_obj(ta);

    lv_screen_load(scr_role);
    UiManager::getInstance()->destroyAllScreensExcept(scr_role);
}

} // namespace user_mgmt
} // namespace ui
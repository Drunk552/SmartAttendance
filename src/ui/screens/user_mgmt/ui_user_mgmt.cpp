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
static lv_obj_t *scr_edit_name = nullptr;
static lv_obj_t *scr_edit_dept = nullptr;
static lv_obj_t *scr_register_password = nullptr;
static lv_obj_t *scr_edit_password = nullptr;

// ================= [内部状态: 输入框指针] =================
static lv_obj_t* g_ta_old_pwd = nullptr;     // 旧密码输入框
static lv_obj_t* g_ta_new_pwd = nullptr;     // 新密码输入框
static lv_obj_t* g_ta_confirm_pwd = nullptr; // 确认密码输入框
static lv_obj_t* g_ta_edit_name = nullptr;   // 编辑姓名输入框
static lv_obj_t* g_ta_edit_dept = nullptr;   // 编辑部门输入
static lv_obj_t* g_ta_new_name = nullptr;   // 注册姓名输入框
static lv_obj_t* g_dd_new_dept = nullptr;   // 注册部门下拉框
static lv_obj_t* g_ta_del_uid = nullptr;     // 工号输入框
static lv_obj_t* g_ta_del_name = nullptr;    // 姓名只读展示框
static lv_obj_t* g_ta_del_dept = nullptr;    // 部门只读展示框

static std::vector<int> g_dept_id_map;//专门用于保存下拉框每一项对应的真实数据库部门 ID

// ================= [内部状态: 控件与数据] =================
static lv_obj_t *obj_list_view = nullptr;
static lv_obj_t *img_face_reg = nullptr;
static lv_obj_t *g_btn_confirm = nullptr;//员工注册按钮
static lv_obj_t* g_btn_del_confirm = nullptr;// 确认删除按钮
static bool s_dept_ready_to_jump = false; // 记录下拉框状态：是否准备跳转

// ================= [内部状态: 注册临时数据暂存] =================
static int g_reg_user_id = 0;
static std::string g_reg_name = "";
static int g_reg_dept_id = 0;
static int g_current_info_uid = 0;// 当前正在查看的员工 ID (用于 info screen)

static bool g_is_updating_face = false;// 标记当前进入摄像头界面是为了“更新老员工”还是“注册新员工”

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
// 一、 员工管理主菜单 (Menu Screen) (一级界面)
// =========================================================

// 员工管理主菜单按钮事件回调
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
            lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---
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

// 员工管理主菜单界面实现
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

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    create_sys_list_btn(list, "1. ", "", "员工列表", user_menu_btn_event_cb, "LIST");
    create_sys_list_btn(list, "2. ", "", "员工注册", user_menu_btn_event_cb, "REG");
    create_sys_list_btn(list, "3. ", "", "删除员工", user_menu_btn_event_cb, "DEL");

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_menu);
    UiManager::getInstance()->destroyAllScreensExcept(scr_menu);// 加载后销毁其他屏幕，保持资源清晰
}

// =========================================================
// 1. 员工列表 (List Screen) (二级界面)
// =========================================================

// 员工列表项事件回调
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
            lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---
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
    lv_obj_set_style_pad_all(parts.content, 5, 0); // 内容区内边距 
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
    lv_obj_set_style_pad_all(obj_list_view, 0, 0);           // 左右上下留一点呼吸空间
    lv_obj_set_style_pad_gap(obj_list_view, 5, 0);            // 列表项之间的间距
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
            lv_obj_set_height(btn, 50); // 固定高度，宽度占满  
            lv_obj_set_style_radius(btn, 0, 0);// 去掉圆角，方形按钮      
            
            lv_obj_add_style(btn, &style_btn_default, 0);// 默认样式
            lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED); // 聚焦样式
            
            // 去除按钮默认的内边距，并开启和表头完全一致的横向布局
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            
            std::string dname = u.dept_name.empty() ? "-" : u.dept_name;// 部门名称可能为空，显示为“-”

            // 第 1 列：工号 (25% 宽度)
            lv_obj_t * l_id = lv_label_create(btn);
            lv_obj_set_width(l_id, LV_PCT(25));
            lv_label_set_long_mode(l_id, LV_LABEL_LONG_DOT); // 如果字太长，自动变成省略号(...)
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

            // 绑定事件时传入 User ID 作为 user_data，方便在回调中识别哪个用户被点击了
            lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_ALL, (void*)(intptr_t)u.id);

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
// 1.1 员工详情 (user info) (三级界面)
// =========================================================

// 员工详情界面事件回调 (兼容点击和键盘事件)
static void user_info_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    // 获取按键值 (如果不是按键事件，key 会是 0)
    if(code==LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 导航逻辑 (兼容键盘方向键)
    if (code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_user_list_screen(); // 返回员工列表界面
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

        // 获取 index (放在这里获取更安全)
        const char* user_data = (const char*)lv_event_get_user_data(e);
        intptr_t index = (intptr_t)user_data;

        if (index == 0){
            show_popup_msg("工号", "不可修改工号! ", nullptr, "我知道了");//不可修改工号
        }
        else if (index == 1) {
            load_user_edit_name_screen(); //跳转到修改姓名界面
        } 
        else if (index == 2) {
            load_user_edit_dept_screen();//跳转到修改部门界面
        }
        else if (index == 3) {
            g_is_updating_face = true;// 告诉摄像头界面：这次是来“更新人脸”的
            load_face_photograph_screen();//跳转到注册人脸界面
        }
        else if (index == 4) {
            //跳转到修改指纹界面
        }
        else if (index == 5) {
            //跳转到修改卡号界面
        }
        else if (index == 6) {
            UserData user = UiController::getInstance()->getUserInfo(g_current_info_uid);
            // 如果密码为空，跳转到“注册密码”
            if (user.password.empty()) {
                load_user_register_password_screen();
            } 
            // 如果已有密码，跳转到“修改密码”
            else {
                load_user_edit_password_screen();
            }
        }
        else if (index == 7) {
            //跳转到修改权限界面
        }
        else {
            
        }
    }
}

// 员工详情界面实现
void load_user_info_screen(int user_id) {

    g_current_info_uid = user_id;// 存储当前查看的用户 ID，方便在修改界面使用

    if (scr_info) {
        lv_obj_delete(scr_info);
        scr_info = nullptr;
    }
    
    UserData user = UiController::getInstance()->getUserInfo(user_id);// 先获取用户数据，确保用户存在
    if (user.id == 0) {
        show_popup("Error", "User Not Found!");
        load_user_list_screen();// 返回员工列表界面
        return;
    }

    BaseScreenParts parts = create_base_screen("员工详情");
    scr_info = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_INFO, &scr_info);
    
    // 绑定销毁回调
    lv_obj_add_event_cb(scr_info, [](lv_event_t * e) {
        scr_info = nullptr;
    }, LV_EVENT_DELETE, NULL);
    
    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件
    
    //UserData u = UiController::getInstance()->getUserInfo(user_id);// 从业务层获取用户数据
    
    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 准备各项需要显示的数据文本
    std::string str_id    = std::to_string(user.id);
    std::string str_name  = user.name;
    // 假设部门有名称映射，如果没有可以直接显示 ID
    std::string str_dept  = UiController::getInstance()->getDeptNameById(user.dept_id); 
    // 判断生物特征是否已录入 (这里假设非空代表已录入)
    std::string str_face  = user.face_feature.empty() ? "未注册" : "已注册";
    std::string str_fp    = user.fingerprint_feature.empty() ? "未注册" : "已注册";
    std::string str_card  = user.card_id.empty() ? "未绑定" : user.card_id;
    std::string str_pwd   = user.password.empty() ? "未注册" : "***";
    std::string str_role  = (user.role == 1) ? "管理员" : "普通";

    int* pass_id = new int(user_id);// 需要在事件回调中使用用户 ID，所以放在堆上并传递指针

    // 第0行：工号
    create_sys_list_btn(list, "1.", "工号", str_id.c_str(), user_info_event_cb, (const char*)(intptr_t)0);
    //第2行： 姓名
    create_sys_list_btn(list, "2.", "姓名", str_name.c_str(), user_info_event_cb, (const char*)(intptr_t)1);
    // 第3行：部门
    create_sys_list_btn(list, "3.", "部门", str_dept.c_str(), user_info_event_cb, (const char*)(intptr_t)2);
    // 第4行：人脸
    create_sys_list_btn(list, "4.", "人脸", str_face.c_str(), user_info_event_cb, (const char*)(intptr_t)3);
    // 第5行：指纹
    create_sys_list_btn(list, "5.", "指纹", str_fp.c_str(), user_info_event_cb, (const char*)(intptr_t)4);
    // 第6行：卡号
    create_sys_list_btn(list, "6.", "卡号", str_card.c_str(), user_info_event_cb, (const char*)(intptr_t)5);
    // 第7行：密码 (这里可以绑定去改密码页的回调，传递用户 ID)
    create_sys_list_btn(list, "7.", "密码", str_pwd.c_str(), user_info_event_cb, (const char*)(intptr_t)6);
    // 第8行：权限 (这里可以绑定去改权限页的回调，传递用户 ID)
    create_sys_list_btn(list, "8.", "权限", str_role.c_str(), user_info_event_cb, (const char*)(intptr_t)7);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 防止内存泄漏：当 list 销毁时，释放 pass_id
    lv_obj_add_event_cb(list, [](lv_event_t* e){
        int* id_ptr = (int*)lv_event_get_user_data(e);
        if(id_ptr) delete id_ptr;
    }, LV_EVENT_DELETE, pass_id);

    lv_screen_load(scr_info);
    UiManager::getInstance()->destroyAllScreensExcept(scr_info);// 加载后销毁其他屏幕，保持资源清晰

}


// =========================================================
// 1.1.n 修改员工信息 (deit user message) (四级界面)
// =========================================================

// ========================== 1.1.1. 修改员工姓名 ===============================

// 修改姓名事件回调 (兼容点击和键盘事件)
static void edit_name_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 
    lv_obj_t *ta_new = (lv_obj_t *)lv_event_get_user_data(e); 

    uint32_t key = 0;
    // 获取按键值
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回员工详情页)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); // 【防连跳】防止 ESC 穿透到上一个界面
        load_user_info_screen(g_current_info_uid);
        return;
    }

    // 2. 区分当前焦点所在控件
    if (current_target == ta_new) {
        // ================= 当前焦点在【新姓名输入框】 =================
        if (code == LV_EVENT_KEY) {
            // 按下回车键(ENTER) 或 下键(DOWN)，转移焦点到确认按钮
            if (key == LV_KEY_ENTER || key == LV_KEY_DOWN) {
                lv_group_t *group = lv_obj_get_group(current_target);
                if (group != nullptr) {
                    lv_group_focus_next(group); 
                    lv_indev_wait_release(lv_indev_get_act());// 【防连跳】防止回车键穿透到下一个界面
                }
            }
        }
    } else {
        // ================= 当前焦点在【确认修改按钮】 =================
        if (code == LV_EVENT_KEY) {
            // 按下上键(UP)，焦点回到新姓名输入框
            if (key == LV_KEY_UP && ta_new != nullptr) {
                lv_group_focus_obj(ta_new); 
                return; // 处理完焦点切换直接返回
            }
        }

        // 3. 处理“确认修改”逻辑
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            
            // 【防连跳】防止在这里按下的回车键穿透到接下来要加载的“员工详情页”里
            lv_indev_wait_release(lv_indev_get_act());

            if (ta_new != nullptr) {
                const char* new_name = lv_textarea_get_text(ta_new);

                if (strlen(new_name) == 0) {
                    show_popup_msg("修改失败", "新姓名不能为空! ", ta_new, "我知道了");
                    return;
                }

                // 调用数据库更新逻辑
                UiController::getInstance()->updateUserName(g_current_info_uid, new_name); 

                // 修改成功后，重新加载员工详情页
                load_user_info_screen(g_current_info_uid);
            }
        }
    }
}

// 修改姓名界面
void load_user_edit_name_screen() {

    UserData user = UiController::getInstance()->getUserInfo(g_current_info_uid);

    if (scr_edit_name){
        lv_obj_delete(scr_edit_name);
        scr_edit_name = nullptr;
    }

    BaseScreenParts parts = create_base_screen("修改姓名");
    scr_edit_name = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_EDIT_NAME, &scr_edit_name);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_edit_name, [](lv_event_t * e) {
        scr_edit_name = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 放入表单组件
    create_form_input(form_cont, "原始姓名:", nullptr, user.name.c_str(), true);
    lv_obj_t* ta_new = create_form_input(form_cont, "新姓名:", "请输入新姓名:", nullptr, false);
    
    // 绑定事件与焦点
    lv_obj_add_event_cb(ta_new, edit_name_event_cb, LV_EVENT_ALL, ta_new);
    UiManager::getInstance()->addObjToGroup(ta_new);

    // 创建确认按钮，并手动加入按键组
    lv_obj_t* btn_confirm = create_form_btn(form_cont, "确认修改", edit_name_event_cb, ta_new);
    UiManager::getInstance()->addObjToGroup(btn_confirm);

    lv_group_focus_obj(ta_new); 
    lv_screen_load(scr_edit_name);
    UiManager::getInstance()->destroyAllScreensExcept(scr_edit_name);
}

// ========================== 1.1.2. 修改员工部门 ===============================

// 修改部门事件回调 (兼容点击和键盘事件)
static void edit_dept_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 
    lv_obj_t *dd_new = (lv_obj_t *)lv_event_get_user_data(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (防连跳拦截)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        // 如果下拉框处于展开状态，按 ESC 会让 LVGL 自动收起它，这里要拦截一下避免直接退出界面
        if (current_target == dd_new && lv_dropdown_is_open(dd_new)) {
            return; // 让 LVGL 内部事件去收起下拉框
        }
        lv_indev_wait_release(lv_indev_get_act()); 
        load_user_info_screen(g_current_info_uid);
        return;
    }

    // 2. 区分焦点所在控件
    if (current_target == dd_new) {
        // ================= 当前焦点在【新部门下拉框】 =================
        
        // 核心交互：当用户在下拉列表中按回车选中某项并收起下拉框时，会触发 VALUE_CHANGED
        if (code == LV_EVENT_VALUE_CHANGED) {
            lv_group_t *group = lv_obj_get_group(current_target);
            if (group != nullptr) {
                lv_group_focus_next(group); // 跳转到确认按钮
                lv_indev_wait_release(lv_indev_get_act()); // 防连跳
            }
        }
        // 如果在未展开时按下了 DOWN 键，也允许跳转焦点到确认按钮
        else if (code == LV_EVENT_KEY && key == LV_KEY_DOWN) {
            if (!lv_dropdown_is_open(dd_new)) {
                lv_group_t *group = lv_obj_get_group(current_target);
                if (group != nullptr) {
                    lv_group_focus_next(group); 
                    lv_indev_wait_release(lv_indev_get_act());
                }
            }
        }

    } else {
        // ================= 当前焦点在【确认修改按钮】 =================
        if (code == LV_EVENT_KEY) {
            // 按下上键(UP)，焦点回到新部门下拉框
            if (key == LV_KEY_UP && dd_new != nullptr) {
                lv_group_focus_obj(dd_new); 
                return; 
            }
        }

        // 3. 处理“确认修改”逻辑
        if (code == LV_EVENT_CLICKED) {
            lv_indev_wait_release(lv_indev_get_act());

            if (dd_new != nullptr) {
                // 获取当前下拉框选中的索引
                uint16_t selected_index = lv_dropdown_get_selected(dd_new);
                
                // 获取部门列表以匹配对应的 ID
                std::vector<DeptInfo> depts = UiController::getInstance()->getDepartmentList();
                if (selected_index < depts.size()) {
                    int new_dept_id = depts[selected_index].id;

                    UiController::getInstance()->updateUserDept(g_current_info_uid, new_dept_id);

                    // 修改成功后，重新加载员工详情页
                    load_user_info_screen(g_current_info_uid);
                }
            }
        }
    }
}

// 修改部门界面
void load_user_edit_dept_screen() {

    UserData user = UiController::getInstance()->getUserInfo(g_current_info_uid);

    if (scr_edit_dept){
        lv_obj_delete(scr_edit_dept);
        scr_edit_dept = nullptr;
    }

    BaseScreenParts parts = create_base_screen("修改部门");
    scr_edit_dept = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_MGMT, &scr_edit_dept);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_edit_dept, [](lv_event_t * e) {
        scr_edit_dept = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 原始部门展示 (复用你现有的文本框方式)
    create_form_input(form_cont, "原始部门:", nullptr, user.dept_name.c_str(), true);
    
    //// 1. 从控制器获取原生数据
    std::vector<DeptInfo> depts = UiController::getInstance()->getDepartmentList();

    // 2. 转换为通用 items 格式
    std::vector<std::pair<int, std::string>> dept_items;
    for (const auto& d : depts) {
        dept_items.push_back({d.id, d.name}); // 存入 ID 和 部门名称
    }
    // 3. 创建部门下拉框
    lv_obj_t* dd_new = create_form_dropdown(form_cont, "新部门:", dept_items, user.dept_id);

    // 绑定事件与焦点
    lv_obj_add_event_cb(dd_new, edit_dept_event_cb, LV_EVENT_ALL, dd_new);
    UiManager::getInstance()->addObjToGroup(dd_new);

    // 创建确认按钮，并手动加入按键组
    lv_obj_t* btn_confirm = create_form_btn(form_cont, "确认修改", edit_dept_event_cb, dd_new);
    UiManager::getInstance()->addObjToGroup(btn_confirm);

    lv_group_focus_obj(dd_new); 
    lv_screen_load(scr_edit_dept);
    UiManager::getInstance()->destroyAllScreensExcept(scr_edit_dept);
}

// ========================== 1.1.3. 注册/修改员工密码 ===============================

// 注册密码事件回调 (兼容点击和键盘事件)
static void register_password_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    // 获取按键值
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回员工详情页)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); // 【防连跳】防止 ESC 穿透
        load_user_info_screen(g_current_info_uid);
        return;
    }

    // 2. 区分当前焦点所在控件
    if (current_target == g_ta_new_pwd || current_target == g_ta_confirm_pwd) {
        // ================= 当前焦点在【密码输入框】 =================
        if (code == LV_EVENT_KEY) {
            // 按下回车键(ENTER) 或 下键(DOWN)，转移焦点到下一个控件
            if (key == LV_KEY_ENTER || key == LV_KEY_DOWN) {
                lv_group_t *group = lv_obj_get_group(current_target);
                if (group != nullptr) {
                    lv_group_focus_next(group); 
                    lv_indev_wait_release(lv_indev_get_act());// 防连跳
                }
            }
            // 按下上键(UP)，转移焦点到上一个控件
            else if (key == LV_KEY_UP) {
                lv_group_t *group = lv_obj_get_group(current_target);
                if (group != nullptr) {
                    lv_group_focus_prev(group);
                    lv_indev_wait_release(lv_indev_get_act());
                }
            }
        }
    } else {
        // ================= 当前焦点在【确认注册按钮】 =================
        if (code == LV_EVENT_KEY) {
            // 按下上键(UP)，焦点回到最后一个输入框（确认密码框）
            if (key == LV_KEY_UP && g_ta_confirm_pwd != nullptr) {
                lv_group_focus_obj(g_ta_confirm_pwd); 
                return; // 处理完焦点切换直接返回
            }
        }

        // 3. 处理“确认注册”逻辑
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            
            // 【防连跳】防止在这里按下的回车键穿透到接下来要加载的界面里
            lv_indev_wait_release(lv_indev_get_act());

            const char* pwd1 = lv_textarea_get_text(g_ta_new_pwd);
            const char* pwd2 = lv_textarea_get_text(g_ta_confirm_pwd);

            if (strlen(pwd1) == 0) return;
            if (strcmp(pwd1, pwd2) != 0) return;

            // 调用数据库更新逻辑
            UiController::getInstance()->updateUserPassword(g_current_info_uid, pwd1); 

            // 修改成功后，重新加载员工详情页
            load_user_info_screen(g_current_info_uid);
        }
    }
}

// 注册密码界面
void load_user_register_password_screen() {

    if (scr_register_password){
        lv_obj_delete(scr_register_password);
        scr_register_password = nullptr;
    }

    BaseScreenParts parts = create_base_screen("密码登记");
    scr_register_password = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_REGISTER_PASSWORD, &scr_register_password);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_register_password, [](lv_event_t * e) {
        scr_register_password = nullptr;
        g_ta_new_pwd = nullptr;     // 清理指针防止野指针
        g_ta_confirm_pwd = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 放入表单组件
    g_ta_new_pwd = create_form_input(form_cont, "输入密码:", "请输入密码：", nullptr, false);
    g_ta_confirm_pwd = create_form_input(form_cont, "确认密码:", "请再次输入密码：", nullptr, false);
    
    // 设置为密码模式（显示为星号 *）
    lv_textarea_set_password_mode(g_ta_new_pwd, true);
    lv_textarea_set_password_mode(g_ta_confirm_pwd, true);

    // 2. 将输入框按顺序加入输入组
    UiManager::getInstance()->addObjToGroup(g_ta_new_pwd);
    UiManager::getInstance()->addObjToGroup(g_ta_confirm_pwd);

    // 绑定事件与焦点
    lv_obj_add_event_cb(g_ta_new_pwd, register_password_event_cb, LV_EVENT_ALL, g_ta_new_pwd);
    lv_obj_add_event_cb(g_ta_confirm_pwd, register_password_event_cb, LV_EVENT_ALL, g_ta_confirm_pwd);

    // 创建确认按钮，并手动加入按键组
    lv_obj_t* btn_confirm = create_form_btn(form_cont, "确认注册", register_password_event_cb, nullptr);
    UiManager::getInstance()->addObjToGroup(btn_confirm);

    lv_group_focus_obj(g_ta_new_pwd); 
    lv_screen_load(scr_register_password);
    UiManager::getInstance()->destroyAllScreensExcept(scr_register_password);
}

//修改密码事件回调 (兼容点击和键盘事件)
static void edit_password_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    // 获取按键值
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回员工详情页)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_user_info_screen(g_current_info_uid);
        return;
    }

    // 2. 区分当前焦点所在控件 (判断三个输入框)
    if (current_target == g_ta_old_pwd || current_target == g_ta_new_pwd || current_target == g_ta_confirm_pwd) {
        // ================= 当前焦点在【密码输入框】 =================
        if (code == LV_EVENT_KEY) {
            // 按下回车键(ENTER) 或 下键(DOWN)，转移焦点到下一个
            if (key == LV_KEY_ENTER || key == LV_KEY_DOWN) {
                lv_group_t *group = lv_obj_get_group(current_target);
                if (group != nullptr) {
                    lv_group_focus_next(group); 
                    lv_indev_wait_release(lv_indev_get_act());
                }
            }
            // 按下上键(UP)，焦点上移
            else if (key == LV_KEY_UP) {
                lv_group_t *group = lv_obj_get_group(current_target);
                if (group != nullptr) {
                    lv_group_focus_prev(group);
                    lv_indev_wait_release(lv_indev_get_act());
                }
            }
        }
    } else {
        // ================= 当前焦点在【确认修改按钮】 =================
        if (code == LV_EVENT_KEY) {
            // 按下上键(UP)，焦点回到最后一个输入框（确认新密码框）
            if (key == LV_KEY_UP && g_ta_confirm_pwd != nullptr) {
                lv_group_focus_obj(g_ta_confirm_pwd); 
                return; 
            }
        }

        // 3. 处理“确认修改”逻辑
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            
            lv_indev_wait_release(lv_indev_get_act());

            const char* old_pwd = lv_textarea_get_text(g_ta_old_pwd);
            const char* new_pwd = lv_textarea_get_text(g_ta_new_pwd);
            const char* confirm_pwd = lv_textarea_get_text(g_ta_confirm_pwd);

            // 业务校验逻辑
            UserData user = UiController::getInstance()->getUserInfo(g_current_info_uid);
            if (strcmp(old_pwd, user.password.c_str()) != 0) {
                show_popup_msg("修改失败", "旧密码不正确! ", g_ta_old_pwd, "我知道了");
                return;
            }
            if (strlen(new_pwd) == 0) {
                show_popup_msg("修改失败", "新密码不能为空! ", g_ta_new_pwd ,"我知道了");
                return;
            }
            if (strcmp(new_pwd, confirm_pwd) != 0) {
                show_popup_msg("修改失败", "两次新密码不一致! ", g_ta_confirm_pwd, "我知道了");
                return;
            }

            // 调用数据库更新逻辑
            UiController::getInstance()->updateUserPassword(g_current_info_uid, new_pwd); 

            // 修改成功后，重新加载员工详情页
            load_user_info_screen(g_current_info_uid);
        }
    }
}

// 修改密码界面
void load_user_edit_password_screen() {

    if (scr_edit_password){ // 这里可以复用同一个屏幕指针，或者如果你定义了 scr_modify_password 就用那个
        lv_obj_delete(scr_edit_password);
        scr_edit_password = nullptr;
    }

    BaseScreenParts parts = create_base_screen("修改密码");
    scr_edit_password = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::USER_EDIT_PASSWORD, &scr_edit_password); // 类型可复用

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_edit_password, [](lv_event_t * e) {
        scr_edit_password = nullptr;
        g_ta_old_pwd = nullptr;     // 清理指针
        g_ta_new_pwd = nullptr;
        g_ta_confirm_pwd = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();
    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 创建三个输入框
    g_ta_old_pwd = create_form_input(form_cont, "旧密码:", "请输入原密码", nullptr, false);
    g_ta_new_pwd = create_form_input(form_cont, "新密码:", "请输入新密码", nullptr, false);
    g_ta_confirm_pwd = create_form_input(form_cont, "确认密码:", "请再次输入新密码", nullptr, false);
    
    // 设置为密码模式（显示为星号 *）
    lv_textarea_set_password_mode(g_ta_old_pwd, true);
    lv_textarea_set_password_mode(g_ta_new_pwd, true);
    lv_textarea_set_password_mode(g_ta_confirm_pwd, true);

    // 2. 按顺序加入输入组
    UiManager::getInstance()->addObjToGroup(g_ta_old_pwd);
    UiManager::getInstance()->addObjToGroup(g_ta_new_pwd);
    UiManager::getInstance()->addObjToGroup(g_ta_confirm_pwd);

    // 绑定事件回调
    lv_obj_add_event_cb(g_ta_old_pwd, edit_password_event_cb, LV_EVENT_KEY, nullptr);
    lv_obj_add_event_cb(g_ta_new_pwd, edit_password_event_cb, LV_EVENT_KEY, nullptr);
    lv_obj_add_event_cb(g_ta_confirm_pwd, edit_password_event_cb, LV_EVENT_KEY, nullptr);

    // 3. 创建确认按钮 (绑定 btn_modify_password_cb)
    lv_obj_t* btn_confirm = create_form_btn(form_cont, "确认修改", edit_password_event_cb, nullptr);
    UiManager::getInstance()->addObjToGroup(btn_confirm);
    lv_obj_add_event_cb(btn_confirm, edit_password_event_cb, LV_EVENT_KEY, nullptr);

    lv_group_focus_obj(g_ta_old_pwd); // 默认焦点在旧密码框
    lv_screen_load(scr_edit_password);
    UiManager::getInstance()->destroyAllScreensExcept(scr_edit_password);
}


// ========================== 1.1.4. 修改员工权限 ===============================

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


// =========================================================
// 2. 员工注册 (user register) (二级界面)
// =========================================================

// 员工注册页面事件回调
static void register_user_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_user_menu_screen(); 
        return;
    }

    // 2. 区分当前焦点所在控件
    if (current_target == g_ta_new_name) {
        // ================= 焦点在【姓名输入框】 =================
        if (code == LV_EVENT_KEY) {
            if (key == LV_KEY_ENTER || key == LV_KEY_DOWN) {
                lv_group_focus_obj(g_dd_new_dept); // 跳到下拉框
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    } 
    else if (current_target == g_dd_new_dept) {
        // ================= 焦点在【部门下拉框】 =================
        if (code == LV_EVENT_KEY) {
            
            if (!lv_dropdown_is_open(g_dd_new_dept)) {
                // 【状态 A：下拉框未展开】
                if (key == LV_KEY_UP) {
                    lv_group_focus_obj(g_ta_new_name); // 按 ↑：回到姓名
                    lv_event_stop_processing(e);       // 切断事件，防止被下拉框底层吃掉！
                    return;
                } else if (key == LV_KEY_DOWN) {
                    lv_group_focus_obj(g_btn_confirm); // 按 ↓：跳到确认按钮
                    lv_event_stop_processing(e);       // 切断事件，防止被下拉框底层吃掉导致展开！
                    return;
                }
                // 注意：这里我们故意不拦截 LV_KEY_ENTER，让 LVGL 正常处理，从而“展开”下拉框。
            } 
            else {
                // 【状态 B：下拉框已展开】
                // 此时 ↑ ↓ 键用于在列表中挑选，不需要拦截。
                if (key == LV_KEY_ENTER) {
                    // 按下回车，LVGL 底层会选中列表项并自动关闭下拉框。
                    // 此时我们不拦截，但我们在下一帧利用异步调用，自动把焦点甩给“确认按钮”！
                    lv_async_call([](void*){
                        if (g_btn_confirm && lv_obj_is_valid(g_btn_confirm)) {
                            lv_group_focus_obj(g_btn_confirm);
                        }
                    }, nullptr);
                }
            }
        }
    } 
    else if (current_target == g_btn_confirm) {
        // ================= 焦点在【确认按钮】 =================
        if (code == LV_EVENT_KEY) {
            if (key == LV_KEY_UP) {
                lv_group_focus_obj(g_dd_new_dept); // 按 ↑ 回到下拉框
                return; 
            }
        }

        // 处理“确认注册”逻辑
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            if (!g_ta_new_name || !g_dd_new_dept) return;

            // 获取填写的姓名
            const char* name_str = lv_textarea_get_text(g_ta_new_name);
            if (name_str == nullptr || strlen(name_str) == 0) {
                show_popup_msg("注册失败", "姓名不能为空! ", g_ta_new_name ,"我知道了");//弹窗提示：姓名不能为空
                return; 
            }

            // 把姓名和部门保存到全局暂存变量中！
            g_reg_name = name_str;
            //g_reg_dept_id = lv_dropdown_get_selected(g_dd_new_dept); // 保存下拉框选中的索引(ID)

            // 1. 获取下拉框选中的【索引】（0, 1, 2...）
            uint16_t selected_index = lv_dropdown_get_selected(g_dd_new_dept); 
            
            // 2. 通过映射表 g_dept_id_map，将【索引】转换成真实的【数据库部门ID】
            if (selected_index < g_dept_id_map.size()) {
                g_reg_dept_id = g_dept_id_map[selected_index];
            } else {
                g_reg_dept_id = 0; // 兜底防错
            }

            load_face_photograph_screen();// 校验并保存完毕后跳转到注册人脸界面
            
        }
    }
}

//注册表单界面实现
void load_user_register_form() {

    if (scr_register){
        lv_obj_delete(scr_register);
        scr_register = nullptr;
    }

    //获取下一个可用工号
    int next_user_id = UiController::getInstance()->generateNextUserId();
    BaseScreenParts parts = create_base_screen("员工注册");
    scr_register = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::REGISTER, &scr_register);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_register, [](lv_event_t * e) {
        scr_register = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 工号：传入转换后的初始文本，并将 is_readonly 设为 true
    std::string id_str = std::to_string(next_user_id);
    lv_obj_t* ta_id = create_form_input(form_cont, "工号:", nullptr, id_str.c_str(), true);

    //lv_textarea_set_text(ta_id, std::to_string(next_user_id).c_str());
    g_ta_new_name = create_form_input(form_cont, "姓名:", "请输入员工姓名", nullptr, false);
    lv_obj_add_event_cb(g_ta_new_name, register_user_event_cb, LV_EVENT_ALL, nullptr); // 使用统一回调，监听 ALL
    UiManager::getInstance()->addObjToGroup(g_ta_new_name); // 加入焦点组

    // 从控制器获取原生数据
    std::vector<DeptInfo> depts = UiController::getInstance()->getDepartmentList();
    std::vector<std::pair<int, std::string>> dept_items;
    g_dept_id_map.clear();//每次加载页面时，清空旧的映射数据

    // 提取默认部门ID (如果数据库为空则默认为0，否则取第一个部门)
    int default_dept_id = depts.empty() ? 0 : depts[0].id;

    for (const auto& d : depts) {
        dept_items.push_back({d.id, d.name}); 
        g_dept_id_map.push_back(d.id);
    }

    g_dd_new_dept = create_form_dropdown(form_cont, "部门:", dept_items, default_dept_id);
    lv_obj_add_event_cb(g_dd_new_dept, register_user_event_cb, LV_EVENT_ALL, nullptr); // 使用统一回调，监听 ALL 
    UiManager::getInstance()->addObjToGroup(g_dd_new_dept); // 加入焦点组

    g_btn_confirm = create_form_btn(form_cont, "确认注册", register_user_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_confirm, register_user_event_cb, LV_EVENT_ALL, nullptr); // 使用统一回调，监听 ALL 
    UiManager::getInstance()->addObjToGroup(g_btn_confirm); // 加入焦点组

    lv_group_focus_obj(g_ta_new_name); 

    lv_screen_load(scr_register);
    UiManager::getInstance()->destroyAllScreensExcept(scr_register);
}

// =========================================================
// 2.1 人脸录入与更新 (face_photograph/updata) (三级界面)
// =========================================================

//人脸拍照界面事件回调
static void face_photograph_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        // --- 情况 A: 按下回车 (注册/更新) ---
        if (key == LV_KEY_ENTER) {
            lv_indev_wait_release(lv_indev_get_act()); // 【防连跳】防止长按回车触发多次数据库写入

            bool success = false;
            
            // 区分保存逻辑
            if (g_is_updating_face) {
                // 调用更新人脸接口 
                success = UiController::getInstance()->updateUserFace(g_current_info_uid);
            } else {
                // 这里就是把第一步表单存下来的姓名(g_reg_name)和部门(g_reg_dept_id)传给底层接口！
                success = UiController::getInstance()->registerNewUser(g_reg_name, g_reg_dept_id);
            }

            if (success) {
                // 成功提示
                show_popup_msg(g_is_updating_face ? "更新人脸" : "员工注册", g_is_updating_face ? "人脸更新成功! " : "员工注册成功! ", nullptr, "确认");
                // 1.5秒后返回相应菜单
                lv_timer_create([](lv_timer_t* t){
                    if (g_is_updating_face) {
                        g_is_updating_face = false; // 用完后立刻重置标志位
                        load_user_info_screen(g_current_info_uid); // 返回详情页
                    } else {
                        // 注册成功，清空暂存的表单数据，防止数据污染下一次注册
                        g_reg_name = "";
                        g_reg_dept_id = 0; // 如果你这里是 string 类型，请改成 ""
                        load_user_menu_screen(); // 注册成功返回菜单
                    }
                    lv_timer_del(t);
                }, 1500, nullptr);
            } else {
                // 失败提示
                show_popup_msg(g_is_updating_face ? "更新人脸" : "员工注册", g_is_updating_face ? "人脸更新失败! " : "员工注册失败! ", nullptr, "确认");
            }
        }
        // --- 情况 B: 按下 ESC (返回) ---
        else if (key == LV_KEY_ESC) {
            lv_indev_wait_release(lv_indev_get_act()); // 【防连跳】防止退回后误触其他界面
            img_face_reg = nullptr; 
            
            // 区分返回路径
            if (g_is_updating_face) {
                g_is_updating_face = false; // 重置标志位
                load_user_info_screen(g_current_info_uid); // 取消更新，返回详情页
            } else {
                // 取消录入，退回填表界面 (此时 g_reg_name 等变量的数据还在，可以继续修改)
                load_user_register_form(); 
            }
        }
    }
}

//人脸拍照界面实现
void load_face_photograph_screen() {
    if (scr_camera){
        lv_obj_delete(scr_camera);
        scr_camera = nullptr;
    }

    //根据动态获取标题
    BaseScreenParts parts = create_base_screen(g_is_updating_face ? "更新人脸" : "注册拍照");
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
 
    // 动态显示用户名字和提示
    lv_obj_t* lbl_hint = lv_label_create(parts.content);
    std::string display_name;
    if (g_is_updating_face) {
        // 如果是更新，获取当前详情页员工的名字
        UserData user = UiController::getInstance()->getUserInfo(g_current_info_uid);
        display_name = user.name;
    } else {
        // 如果是注册，使用第一步暂存的新名字
        display_name = g_reg_name;
    }
    lv_label_set_text_fmt(lbl_hint, "Hi, %s!\nPress ENTER to %s", 
                          display_name.c_str(), 
                          g_is_updating_face ? "Update" : "Register");
    lv_obj_set_style_text_align(lbl_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_hint, lv_color_white(), 0);
    lv_obj_align_to(lbl_hint, img_face_reg, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // 绑定提取出来的按键回调函数
    lv_obj_add_event_cb(img_face_reg, face_photograph_event_cb, LV_EVENT_KEY, nullptr);

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


// =========================================================
// 3. 删除员工 (delete user) (二级界面)
// =========================================================

// 删除员工事件回调
static void delete_user_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回员工菜单)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_user_menu_screen(); //员工管理界面
        return;
    }

    // ================= 焦点在【工号输入框】 =================
    if (current_target == g_ta_del_uid) {
        // 输入框内容改变时，自动查询数据库
        if (code == LV_EVENT_VALUE_CHANGED) {
            const char* uid_str = lv_textarea_get_text(g_ta_del_uid);
            int input_uid = atoi(uid_str); // 转换为整型工号
            
            if (input_uid > 0) {
                // 调用接口获取用户信息
                UserData user = UiController::getInstance()->getUserInfo(input_uid);
                
                if (!user.name.empty()) {
                    // 用户存在，自动把数据填入不可写的姓名和部门框
                    lv_textarea_set_text(g_ta_del_name, user.name.c_str());
                    lv_textarea_set_text(g_ta_del_dept, user.dept_name.c_str()); 
                } else {
                    // 用户不存在，清空下面两个框
                    lv_textarea_set_text(g_ta_del_name, "");
                    lv_textarea_set_text(g_ta_del_dept, "");
                }
            } else {
                // 输入不合法或被删空时，清空数据
                lv_textarea_set_text(g_ta_del_name, "");
                lv_textarea_set_text(g_ta_del_dept, "");
            }
        }
        // [按键跳转]：按下回车或↓键，跳到确认删除按钮
        else if (code == LV_EVENT_KEY) {
            if (key == LV_KEY_ENTER || key == LV_KEY_DOWN) {
                lv_group_focus_obj(g_btn_del_confirm);
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    } 
    // ================= 焦点在【确认删除按钮】 =================
    else if (current_target == g_btn_del_confirm) {
        // [按键跳转]：按下↑键，跳回到工号输入框
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_del_uid);
            return; 
        }

        // [业务逻辑]：按下确认删除
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            const char* uid_str = lv_textarea_get_text(g_ta_del_uid);
            int input_uid = atoi(uid_str);
            const char* name_str = lv_textarea_get_text(g_ta_del_name);
            
            // 校验：如果姓名框为空，说明没获取到真实存在的用户
            if (input_uid <= 0 || strlen(name_str) == 0) {
                show_popup_msg("删除失败", "该用户不存在！", g_ta_del_uid);
                return;
            }

            // 执行删除逻辑
            bool success = UiController::getInstance()->deleteUser(input_uid);
            if (success) {
                show_popup_msg("删除成功", "用户已成功删除！", nullptr);
                load_user_menu_screen(); // 删除成功返回菜单
            } else {
                show_popup_msg("失败", "删除失败，请重试！", g_ta_del_uid);
            }
        }
    }
}

// 删除员工界面
void load_user_delete_screen() {
    if (scr_del){
        lv_obj_delete(scr_del);
        scr_del = nullptr;
    }

    BaseScreenParts parts = create_base_screen("删除用户");
    scr_del = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DELETE_USER, &scr_del);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_del, [](lv_event_t * e) {
        scr_del = nullptr;
        g_ta_del_uid = nullptr;
        g_ta_del_name = nullptr;
        g_ta_del_dept = nullptr;
        g_btn_del_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    // 绑定全局 ESC 返回事件
    lv_obj_add_event_cb(scr_del, delete_user_event_cb, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建统一表单容器
    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 创建三个输入框 
    // 只有工号可输入，姓名和部门设为只读 (is_readonly = true)
    g_ta_del_uid  = create_form_input(form_cont, "工号:", "请输入要删除的工号：", "", false);
    g_ta_del_name = create_form_input(form_cont, "姓名:", "自动获取姓名：", "", true);
    g_ta_del_dept = create_form_input(form_cont, "部门:", "自动获取部门：", "", true);
    
    lv_textarea_set_accepted_chars(g_ta_del_uid, "0123456789");// 限制工号只能输入数字 
    lv_textarea_set_max_length(g_ta_del_uid, 8);//只可输入8位数字

    // 2. 绑定事件 (只绑定需要交互的控件)
    lv_obj_add_event_cb(g_ta_del_uid, delete_user_event_cb, LV_EVENT_ALL, nullptr);

    // 3. 加入焦点组 (不加入只读框)
    UiManager::getInstance()->addObjToGroup(g_ta_del_uid);

    // 4. 创建删除按钮，并绑定事件与焦点
    g_btn_del_confirm = create_form_btn(form_cont, "确认删除", delete_user_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_del_confirm, delete_user_event_cb, LV_EVENT_KEY, nullptr); // 显式绑定按键以便支持UP键
    UiManager::getInstance()->addObjToGroup(g_btn_del_confirm);
    
    // 应用高危专属样式 
    // 1. 设置默认状态(未聚焦)下为暗红色，起到警示作用
    lv_obj_set_style_bg_color(g_btn_del_confirm, lv_palette_darken(LV_PALETTE_RED, 2), LV_STATE_DEFAULT);
    // 2. 覆盖通用聚焦样式，应用特殊焦点样式
    lv_obj_add_style(g_btn_del_confirm, &style_focus_red, LV_STATE_FOCUSED);

    // 默认焦点在工号输入框
    lv_group_focus_obj(g_ta_del_uid); 
    
    lv_screen_load(scr_del);
    UiManager::getInstance()->destroyAllScreensExcept(scr_del);
}


} // namespace user_mgmt
} // namespace ui
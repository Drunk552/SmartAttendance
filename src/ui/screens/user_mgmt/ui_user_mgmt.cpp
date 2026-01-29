#include "ui_user_mgmt.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h" // 用于返回主菜单

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

// 辅助：统一返回主菜单
static void back_to_main_menu() {
    ui::menu::load_screen(); 
}

void init() {
    g_reg_user_id = 0;
    g_reg_name = "";
}

// =========================================================
// 1. 员工管理菜单 (Menu Screen)
// =========================================================
static void menu_btn_event_cb(lv_event_t *e) {
    const char* tag = (const char*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 物理按键导航逻辑
        if (key == LV_KEY_DOWN) lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
        else if (key == LV_KEY_UP) lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
        else if (key == LV_KEY_ESC) back_to_main_menu();
        
        else if (key == LV_KEY_ENTER) {
            if (strcmp(tag, "LIST") == 0) load_list_screen();
            else if (strcmp(tag, "REG") == 0) load_register_form();
            else if (strcmp(tag, "DEL") == 0) load_delete_user_screen();
        }
    }
}

void load_menu_screen() {
    if (!scr_menu) {
        scr_menu = lv_obj_create(nullptr);
        lv_obj_add_style(scr_menu, &style_base, 0);

        lv_obj_t *title = lv_label_create(scr_menu);
        lv_label_set_text(title, "User Management / 员工管理");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_add_style(title, &style_text_cn, 0);

        static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
        static int32_t row_dsc[] = {70, 70, 70, LV_GRID_TEMPLATE_LAST};
        
        lv_obj_t *grid = lv_obj_create(scr_menu);
        lv_obj_set_size(grid, 220, 240);
        lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_layout(grid, LV_LAYOUT_GRID);
        lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
        lv_obj_add_style(grid, &style_panel_transp, 0);
        lv_obj_set_style_pad_row(grid, 10, 0);

        UiManager::getInstance()->registerScreen(ScreenType::USER_MGMT, &scr_menu);

        create_sys_grid_btn(grid, 0, LV_SYMBOL_LIST, "User List", "员工列表", menu_btn_event_cb, "LIST");
        create_sys_grid_btn(grid, 1, LV_SYMBOL_PLUS, "Register", "员工注册", menu_btn_event_cb, "REG");
        create_sys_grid_btn(grid, 2, LV_SYMBOL_TRASH, "Delete", "删除员工", menu_btn_event_cb, "DEL");
    }

    UiManager::getInstance()->resetKeypadGroup();
    lv_obj_t *grid = lv_obj_get_child(scr_menu, 1); 
    for(uint32_t i=0; i<lv_obj_get_child_cnt(grid); i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(grid, i));
    }
    if(lv_obj_get_child_cnt(grid) > 0) lv_group_focus_obj(lv_obj_get_child(grid, 0));
    UiManager::getInstance()->addObjToGroup(scr_menu);

    lv_screen_load(scr_menu);
    UiManager::getInstance()->destroyAllScreensExcept(scr_menu);
}

// =========================================================
// 2. 员工列表 (List Screen)
// =========================================================
static void list_item_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            int uid = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
            load_user_info_screen(uid);
        } else if (key == LV_KEY_ESC) {
            load_menu_screen();
        } else if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
        } else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
        }
    }
}

void load_list_screen() {
    if (!scr_list) {
        scr_list = lv_obj_create(nullptr);
        lv_obj_add_style(scr_list, &style_base, 0);
        
        lv_obj_t *title = lv_label_create(scr_list);
        lv_label_set_text(title, "员工列表 / User List");
        lv_obj_add_style(title, &style_text_cn, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

        obj_list_view = lv_obj_create(scr_list);
        lv_obj_set_size(obj_list_view, LV_PCT(95), LV_PCT(80));
        lv_obj_align(obj_list_view, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_flex_flow(obj_list_view, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(obj_list_view, THEME_COLOR_PANEL, 0);
        lv_obj_set_style_border_width(obj_list_view, 0, 0);
        lv_obj_set_style_pad_all(obj_list_view, 5, 0);
        lv_obj_set_style_pad_gap(obj_list_view, 5, 0);

        UiManager::getInstance()->registerScreen(ScreenType::USER_LIST, &scr_list);
    }

    lv_obj_clean(obj_list_view);
    UiManager::getInstance()->resetKeypadGroup();

    auto users = UiController::getInstance()->getAllUsers();
    if (users.empty()) {
        lv_obj_t *lbl = lv_label_create(obj_list_view);
        lv_label_set_text(lbl, "暂无员工数据");
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    } else {
        for (const auto& u : users) {
            lv_obj_t *btn = lv_button_create(obj_list_view);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, 45);
            lv_obj_add_style(btn, &style_btn_default, 0);
            
            lv_obj_t *lbl = lv_label_create(btn);
            std::string dname = u.dept_name.empty() ? "-" : u.dept_name;
            lv_label_set_text_fmt(lbl, "%d  |  %s  |  %s", u.id, u.name.c_str(), dname.c_str());
            lv_obj_add_style(lbl, &style_text_cn, 0);
            lv_obj_center(lbl);

            lv_obj_set_user_data(btn, (void*)(intptr_t)u.id);
            lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_KEY, nullptr);
            UiManager::getInstance()->addObjToGroup(btn);
        }
        lv_group_focus_obj(lv_obj_get_child(obj_list_view, 0));
    }
    
    lv_obj_add_event_cb(scr_list, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_menu_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_list);

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

// 注册 Step 1: 加载表单
void load_register_form() {
    if (scr_register) lv_obj_delete(scr_register);
    scr_register = lv_obj_create(NULL);
    
    lv_obj_set_style_bg_color(scr_register, lv_color_hex(0xF0F0F0), 0);
    UiManager::getInstance()->registerScreen(ScreenType::REGISTER, &scr_register);

    lv_obj_t * title = lv_label_create(scr_register);
    lv_label_set_text(title, "员工注册 / Registration");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0); 

    lv_obj_t * form_cont = lv_obj_create(scr_register);
    lv_obj_set_size(form_cont, 220, 180); 
    lv_obj_align(form_cont, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(form_cont, LV_FLEX_FLOW_COLUMN); 
    lv_obj_set_style_pad_all(form_cont, 5, 0);

    // [ID]
    lv_obj_t * lbl_id = lv_label_create(form_cont);
    lv_label_set_text(lbl_id, "ID (Auto):");
    lv_obj_set_style_text_color(lbl_id, lv_color_black(), 0);
    
    lv_obj_t * ta_id = lv_textarea_create(form_cont);
    lv_obj_set_width(ta_id, LV_PCT(100));
    lv_textarea_set_one_line(ta_id, true);
    lv_obj_add_state(ta_id, LV_STATE_DISABLED);
    
    g_reg_user_id = UiController::getInstance()->generateNextUserId();
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "%06d", g_reg_user_id);
    lv_textarea_set_text(ta_id, buf);

    // [Name]
    lv_obj_t * lbl_name = lv_label_create(form_cont);
    lv_label_set_text(lbl_name, "Name / 姓名:");
    lv_obj_add_style(lbl_name, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_name, lv_color_black(), 0);

    lv_obj_t * ta_name = lv_textarea_create(form_cont);
    lv_obj_set_width(ta_name, LV_PCT(100));
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_placeholder_text(ta_name, "Enter Name");
    lv_obj_add_event_cb(ta_name, form_nav_event_cb, LV_EVENT_ALL, NULL);
    
    // [Dept]
    lv_obj_t * lbl_dept = lv_label_create(form_cont);
    lv_label_set_text(lbl_dept, "Dept / 部门:");
    lv_obj_add_style(lbl_dept, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_dept, lv_color_black(), 0);

    lv_obj_t * dd_dept = lv_dropdown_create(form_cont);
    lv_obj_set_width(dd_dept, LV_PCT(100));
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
    lv_obj_set_style_text_color(dd_dept, lv_color_black(), 0);
    lv_obj_add_event_cb(dd_dept, form_nav_event_cb, LV_EVENT_ALL, NULL);

    // [Buttons]
    lv_obj_t * btn_area = lv_obj_create(scr_register);
    lv_obj_remove_style_all(btn_area);
    lv_obj_set_size(btn_area, LV_PCT(100), 40);
    lv_obj_align(btn_area, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_set_flex_flow(btn_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_area, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_cancel = lv_button_create(btn_area);
    lv_obj_set_width(btn_cancel, 80);
    lv_obj_set_style_bg_color(btn_cancel, lv_palette_main(LV_PALETTE_RED), 0);
    lv_label_set_text(lv_label_create(btn_cancel), "Cancel");
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t * e){ load_menu_screen(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_cancel, form_nav_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * btn_next = lv_button_create(btn_area);
    lv_obj_set_width(btn_next, 80);
    lv_obj_set_style_bg_color(btn_next, THEME_COLOR_PRIMARY, 0);
    lv_label_set_text(lv_label_create(btn_next), "Next >");
    
    lv_obj_set_user_data(ta_name, dd_dept); 
    lv_obj_add_event_cb(btn_next, register_btn_next_event_handler, LV_EVENT_CLICKED, ta_name);
    lv_obj_add_event_cb(btn_next, form_nav_event_cb, LV_EVENT_ALL, NULL);

    // [Footer]
    lv_obj_t * bottom_bar = lv_obj_create(scr_register);
    lv_obj_set_size(bottom_bar, LV_PCT(100), 30);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_black(), 0);
    lv_label_set_text(lv_label_create(bottom_bar), "Enter:Select  Up/Down:Nav");
    lv_obj_set_style_text_color(lv_obj_get_child(bottom_bar, 0), lv_color_white(), 0);
    lv_obj_center(lv_obj_get_child(bottom_bar, 0));

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(ta_name);
    UiManager::getInstance()->addObjToGroup(dd_dept);
    UiManager::getInstance()->addObjToGroup(btn_next);
    UiManager::getInstance()->addObjToGroup(btn_cancel);
    lv_group_focus_obj(ta_name);

    lv_screen_load(scr_register);
    UiManager::getInstance()->destroyAllScreensExcept(scr_register);
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

    load_register_camera_step();
}

// 注册 Step 2: 加载拍照界面
void load_register_camera_step() {
    if (!scr_register) return; 
    
    lv_obj_clean(scr_register);
    lv_obj_set_style_bg_color(scr_register, lv_color_black(), 0); 
    UiManager::getInstance()->resetKeypadGroup();
    
    lv_obj_t *label = lv_label_create(scr_register);
    lv_label_set_text(label, "Step 2: Capture Face");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    
    static lv_image_dsc_t img_dsc; 
    img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
    img_dsc.header.w = CAM_W;
    img_dsc.header.h = CAM_H;
    img_dsc.header.stride = CAM_W * 3;
    img_dsc.data_size = CAM_W * CAM_H * 3;
    img_dsc.data = UiManager::getInstance()->getCameraDisplayBuffer();

    img_face_reg = lv_image_create(scr_register);
    lv_image_set_src(img_face_reg, &img_dsc); 
    lv_obj_set_size(img_face_reg, CAM_W, CAM_H);
    lv_obj_align(img_face_reg, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_set_style_border_width(img_face_reg, 3, 0);
    lv_obj_set_style_border_color(img_face_reg, lv_palette_main(LV_PALETTE_GREEN), 0);
    
    lv_obj_add_flag(img_face_reg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img_face_reg, reg_step2_event_cb, LV_EVENT_KEY, nullptr);
    
    lv_obj_t *tip = lv_label_create(scr_register);
    char tip_buf[128];
    std::snprintf(tip_buf, sizeof(tip_buf), "Hi, %s!\nPress ENTER to Capture", g_reg_name.c_str());
    lv_label_set_text(tip, tip_buf);
    lv_obj_set_style_text_color(tip, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    UiManager::getInstance()->addObjToGroup(img_face_reg);
    lv_group_focus_obj(img_face_reg);
}

// 注册 Step 2: 拍照事件回调
static void reg_step2_event_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ENTER) {
        if (UiController::getInstance()->registerNewUser(g_reg_name.c_str(), g_reg_dept_id)) {
            show_popup("Success", "User Registered!");
            lv_timer_t *t = lv_timer_create([](lv_timer_t*){ load_menu_screen(); }, 1500, nullptr);
            lv_timer_set_repeat_count(t, 1);
        } else {
            show_popup("Error", "Registration Failed!\n(Check DB/Dept ID)");
        }
    } else if (lv_event_get_key(e) == LV_KEY_ESC) {
        load_register_form(); 
    }
}

// =========================================================
// 4. 删除用户 (Delete Screen)
// =========================================================

void load_delete_user_screen() {
    if(scr_del) lv_obj_delete(scr_del);
    scr_del = lv_obj_create(nullptr);
    lv_obj_add_style(scr_del, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::DELETE_USER, &scr_del);

    // 标题
    lv_obj_t *title = lv_label_create(scr_del);
    lv_label_set_text(title, "删除员工 / Delete User");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // 输入框
    lv_obj_t *ta_id = lv_textarea_create(scr_del);
    lv_textarea_set_one_line(ta_id, true);
    lv_textarea_set_placeholder_text(ta_id, "输入工号 (ID)");
    lv_textarea_set_accepted_chars(ta_id, "0123456789"); 
    lv_textarea_set_max_length(ta_id, 8);
    lv_obj_set_width(ta_id, LV_PCT(80));
    lv_obj_align(ta_id, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_style(ta_id, &style_focus_red, LV_STATE_FOCUSED);

    // 删除按钮
    lv_obj_t *btn_del = lv_button_create(scr_del);
    lv_obj_set_width(btn_del, LV_PCT(60));
    lv_obj_align(btn_del, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_style(btn_del, &style_focus_red, LV_STATE_FOCUSED);
    
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
                    lv_timer_t *t = lv_timer_create([](lv_timer_t*){ load_menu_screen(); }, 1500, nullptr);
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
        if(lv_event_get_key(e) == LV_KEY_ESC) load_menu_screen();
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
    if(scr_info) lv_obj_delete(scr_info);
    scr_info = lv_obj_create(nullptr);
    lv_obj_add_style(scr_info, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::USER_INFO, &scr_info);

    UserData u = UiController::getInstance()->getUserInfo(user_id);

    // 容器
    lv_obj_t *grid = lv_obj_create(scr_info);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(grid, THEME_COLOR_PANEL, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    // 布局: 2列 x 8行
    static int32_t col_dsc[] = {80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {35, 35, 35, 35, 35, 35, 35, 35, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    UiManager::getInstance()->resetKeypadGroup();

    // 辅助Lambda：创建一行
    auto create_row = [&](int row, const char* label, lv_obj_t* content) {
        lv_obj_t *lbl = lv_label_create(grid);
        lv_label_set_text(lbl, label);
        lv_obj_add_style(lbl, &style_text_cn, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_grid_cell(lbl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
        
        if (content) {
            lv_obj_set_grid_cell(content, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, row, 1);
            // 如果是可交互对象，添加高亮样式和 ESC 处理
            if (lv_obj_has_flag(content, LV_OBJ_FLAG_CLICKABLE) || lv_obj_check_type(content, &lv_textarea_class)) {
                lv_obj_add_style(content, &style_focus_red, LV_STATE_FOCUSED);
                UiManager::getInstance()->addObjToGroup(content);
                // ESC 返回列表
                lv_obj_add_event_cb(content, [](lv_event_t* e){
                    if (lv_event_get_key(e) == LV_KEY_ESC) load_list_screen();
                }, LV_EVENT_KEY, nullptr);
            }
        }
        return content;
    };

    // [1. 工号]
    lv_obj_t *lbl_id = lv_label_create(grid);
    lv_label_set_text_fmt(lbl_id, "%d", u.id);
    lv_obj_set_style_text_color(lbl_id, lv_color_white(), 0);
    create_row(0, "工号", lbl_id);

    // [2. 姓名] (可修改，失去焦点自动保存)
    lv_obj_t *ta_name = lv_textarea_create(grid);
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_text(ta_name, u.name.c_str());
    lv_obj_set_user_data(ta_name, (void*)(intptr_t)u.id);
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e){
         if(lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
             lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
             int uid = (int)(intptr_t)lv_event_get_user_data(e);
             UiController::getInstance()->updateUserName(uid, lv_textarea_get_text(ta));
         }
    }, LV_EVENT_ALL, nullptr);
    create_row(1, "姓名", ta_name);

    // [3. 人脸] (Enter -> 重新录入)
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
            g_reg_user_id = uid; 
            // 获取最新名字以显示在拍照页
            g_reg_name = UiController::getInstance()->getUserInfo(uid).name;
            load_register_camera_step(); // 跳转去拍照
        }
    }, LV_EVENT_KEY, nullptr);
    create_row(2, "人脸", btn_face);

    // [4. 部门]
    lv_obj_t *lbl_dept = lv_label_create(grid);
    lv_label_set_text(lbl_dept, u.dept_name.c_str());
    lv_obj_add_style(lbl_dept, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_dept, lv_color_white(), 0);
    create_row(3, "部门", lbl_dept);

    // [5. 指纹] (占位)
    lv_obj_t *lbl_fp = lv_label_create(grid);
    lv_label_set_text(lbl_fp, "未录入 (暂不支持)");
    lv_obj_add_style(lbl_fp, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_fp, lv_palette_main(LV_PALETTE_GREY), 0);
    create_row(4, "指纹", lbl_fp);

    // [6. 密码] (Enter -> 修改页)
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

    // [7. 卡号]
    lv_obj_t *lbl_card = lv_label_create(grid);
    lv_label_set_text(lbl_card, u.card_id.empty() ? "无" : u.card_id.c_str());
    lv_obj_set_style_text_color(lbl_card, lv_color_white(), 0);
    create_row(6, "卡号", lbl_card);

    // [8. 权限] (Enter -> 权限变更页)
    lv_obj_t *btn_role = lv_button_create(grid);
    lv_obj_t *lbl_role = lv_label_create(btn_role);
    lv_label_set_text(lbl_role, (u.role == 1) ? "管理员" : "普通员工");
    lv_obj_add_style(lbl_role, &style_text_cn, 0);
    lv_obj_center(lbl_role);
    lv_obj_set_user_data(btn_role, (void*)(intptr_t)u.id);
    lv_obj_add_event_cb(btn_role, [](lv_event_t* e){
        if (lv_event_get_key(e) == LV_KEY_ENTER) {
            int uid = (int)(intptr_t)lv_event_get_user_data(e);
            // 重新查询确保数据最新
            int r = UiController::getInstance()->getUserInfo(uid).role;
            load_role_change_screen(uid, r);
        }
    }, LV_EVENT_KEY, nullptr);
    create_row(7, "权限", btn_role);

    // 默认聚焦姓名输入框
    lv_group_focus_obj(ta_name);

    lv_screen_load(scr_info);
    UiManager::getInstance()->destroyAllScreensExcept(scr_info);
}

// =========================================================
// 6. 修改密码页 (Password Change) - 完整双重校验逻辑
// =========================================================

void load_password_change_screen(int user_id) {
    if(scr_pwd) lv_obj_delete(scr_pwd);
    scr_pwd = lv_obj_create(nullptr);
    lv_obj_add_style(scr_pwd, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::PWD_CHANGE, &scr_pwd);

    lv_obj_t *title = lv_label_create(scr_pwd);
    lv_label_set_text(title, "设置密码 / Set Password");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

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
    lv_obj_add_style(p1, &style_focus_red, LV_STATE_FOCUSED);

    // 输入框 2: 确认密码
    lv_obj_t *p2 = lv_textarea_create(cont);
    lv_textarea_set_password_mode(p2, true);
    lv_textarea_set_placeholder_text(p2, "再次输入");
    lv_textarea_set_one_line(p2, true);
    lv_obj_set_width(p2, LV_PCT(100));
    lv_obj_add_style(p2, &style_focus_red, LV_STATE_FOCUSED);

    // 按钮区
    lv_obj_t *btn_box = lv_obj_create(cont);
    lv_obj_set_size(btn_box, LV_PCT(100), 50);
    lv_obj_set_flex_flow(btn_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_box, LV_OPA_TRANSP, 0);

    // 确认按钮
    lv_obj_t *btn_ok = lv_button_create(btn_box);
    lv_label_set_text(lv_label_create(btn_ok), "确认");
    lv_obj_add_style(btn_ok, &style_focus_red, LV_STATE_FOCUSED);
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
    lv_obj_add_style(btn_cancel, &style_focus_red, LV_STATE_FOCUSED);
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

void load_role_change_screen(int user_id, int current_role) {
    if(scr_role) lv_obj_delete(scr_role);
    scr_role = lv_obj_create(nullptr);
    lv_obj_add_style(scr_role, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::ROLE_AUTH, &scr_role);

    lv_obj_t *title = lv_label_create(scr_role);
    lv_label_set_text(title, current_role == 0 ? "设为管理员" : "取消管理员");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *ta = lv_textarea_create(scr_role);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "Admin Password");
    lv_textarea_set_one_line(ta, true);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(ta, &style_focus_red, LV_STATE_FOCUSED);

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
#include "ui_sys_settings.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring> // for strcmp
#include <vector> // for std::vector
#include <string> // for std::string

namespace ui {
namespace system {

// =================菜单--系统设置==================
static lv_obj_t *scr_sys = nullptr;//系统主屏幕
static lv_obj_t *scr_basic = nullptr;//基础设置屏幕
static lv_obj_t *scr_advanced = nullptr;//高级设置屏幕
static lv_obj_t *scr_param = nullptr;//参数设置屏幕
static lv_obj_t *scr_self_check = nullptr;//自检设置屏幕

// =================基础设置-时间设置（三级）==================
static lv_obj_t *scr_basic_settime =nullptr;//时间设置屏幕
static lv_obj_t *g_roller_hour = nullptr;   // 小时滚轮
static lv_obj_t *g_roller_min  = nullptr;   // 分钟滚轮
static lv_obj_t *g_roller_sec  = nullptr;   // 秒滚轮
static void show_time_confirm_dialog(const char* time_str);

// =================基础设置-日期（三级）==================
static lv_obj_t *scr_basic_date =nullptr;//日期屏幕
static lv_obj_t *scr_basic_date_settings =nullptr;//日期设置屏幕
static lv_obj_t *scr_basic_date_format =nullptr;//日期格式屏幕

// =================基础设置-日期设置（四级）==================
static lv_obj_t *scr_basic_date_setting = nullptr;  // 日期设置屏幕
static lv_obj_t *g_ta_year = nullptr;      // 年输入框
static lv_obj_t *g_ta_month = nullptr;     // 月输入框
static lv_obj_t *g_ta_day = nullptr;       // 日输入框
static lv_obj_t *g_roller_date_format = nullptr;   // 日期格式滚轮

// =================基础设置-音量（三级）==================
static lv_obj_t *scr_basic_volume = nullptr;        // 音量设置屏幕
static lv_obj_t *g_slider_volume = nullptr;         // 音量滑块
static lv_obj_t *g_label_volume_value = nullptr;    // 音量数值显示
static lv_obj_t *g_switch_mute = nullptr;           // 静音开关
// 当前音量值（0-100）
static int g_current_volume = 80;
// 静音状态
static bool g_is_muted = false;

// =================基础设置-语言设置（三级）==================
static lv_obj_t *scr_basic_language = nullptr;        // 语言设置屏幕
static lv_obj_t *g_dropdown_language = nullptr;         // 语言选择

// =================基础设置-屏保时间设置（三级）==================
static lv_obj_t *scr_basic_screensafe = nullptr;      // 屏保设置屏幕
static int g_current_screensafe_time = 30;

// =================基础设置-机器号设置（三级）==================
static lv_obj_t *scr_basic_machine_id = nullptr;
static lv_obj_t *g_ta_machine_id = nullptr;

// =================基础设置-返回主界面时间设置（三级）================
static lv_obj_t *scr_basic_return_time = nullptr;
static lv_obj_t *g_roller_return_time = nullptr;

// =================基础设置-管理员总数设置（三级）================
static lv_obj_t *scr_basic_admin_count = nullptr;
static lv_obj_t *g_ta_admin_count = nullptr;
static int g_admin_count = 3;  // 默认3个管理员

// =================基础设置-记录警告数设置（三级）================
static lv_obj_t *scr_basic_warn_count = nullptr;
static lv_obj_t *g_roller_warn_count = nullptr;
static lv_obj_t *g_dropdown_warn_count = nullptr;  // 警告记录数下拉框
static lv_obj_t *g_label_warn_preview = nullptr;   // 预览标签
static int g_warn_count_index = 4;  // 默认500条


// ================= 高级设置 =================
static lv_obj_t *scr_advanced_clear_records = nullptr;    // 清空记录界面
static lv_obj_t *scr_advanced_clear_employees = nullptr;  // 清空员工界面
static lv_obj_t *scr_advanced_clear_all_data = nullptr;   // 清除所有数据
static lv_obj_t *scr_advanced_factory_reset = nullptr;    // 恢复出厂设置界面
static lv_obj_t *scr_advanced_upgrade = nullptr;          // 系统升级界面

// =================界面指针==================
static lv_obj_t *g_dialog_confirm = nullptr;        // 对话框容器
static lv_obj_t *g_dialog_btn_yes = nullptr;        // "是"按钮
static lv_obj_t *g_dialog_btn_no = nullptr;         // "否"按钮
static lv_obj_t *g_btn_confirm = nullptr;//确认按钮
static lv_obj_t *g_btn_cancel = nullptr;  // 取消按钮

// =================系统主界面(一级)==================

// 系统设置菜单按钮事件回调
static void sys_main_event_cb(lv_event_t *e) {
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
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());// 向下导航
        } 
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
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
        if (strcmp(tag, "BASIC") == 0)      load_sys_settings_basic_screen();// 基础设置
        else if (strcmp(tag, "ADVANCED") == 0)  load_sys_settings_advanced_screen();// 高级设置
        else if (strcmp(tag, "SELFCHECK") == 0) load_sys_settings_selfcheck_screen();//自检功能
    }
}

// 主屏幕实现
void load_sys_settings_menu_screen() {

    if (scr_sys) {
        lv_obj_delete(scr_sys);
        scr_sys = nullptr;
    }

    BaseScreenParts parts = create_base_screen("系统设置");
    scr_sys = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_SETTINGS, &scr_sys);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_sys, [](lv_event_t * e) {
        scr_sys = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    //1.基础设置
    create_sys_list_btn(list, "1. ", "", "基础设置", sys_main_event_cb, "BASIC");
    // 2.高级设置
    create_sys_list_btn(list, "2. ", "", "高级设置", sys_main_event_cb, "ADVANCED");
    // 3.自检功能
    create_sys_list_btn(list, "3. ", "", "自检功能", sys_main_event_cb, "SELFCHECK");

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_sys);
    UiManager::getInstance()->destroyAllScreensExcept(scr_sys);
}

// =================基础设置界面（二级）================
static void sys_basic_event_cb(lv_event_t *e){
    const char* tag = (const char*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY){

        if(key == LV_KEY_ESC){
          load_sys_settings_menu_screen();
          lv_indev_wait_release(lv_indev_get_act());
          return;
        }

        else if(key == LV_KEY_DOWN || key == LV_KEY_RIGHT){
          lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
          return;
        }

        else if(key == LV_KEY_UP || key == LV_KEY_LEFT){
          lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
          return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)){
        lv_indev_wait_release(lv_indev_get_act());

        if(strcmp (tag, "TIME") == 0)  load_sys_basic_time_settings_screen();
        else if(strcmp (tag, "DATE") == 0)  load_sys_basic_date_screen();
        else if(strcmp (tag, "VOLUME") == 0)  load_sys_basic_volume_settings_screen();
        else if(strcmp (tag, "LANGUAGE") == 0)  load_sys_basic_language_settings_screen();
        else if(strcmp (tag, "SCREENSAFE") == 0)  load_sys_basic_screensafe_settings_screen();
        else if(strcmp (tag, "MACHINE_ID") == 0)  load_sys_basic_machine_id_screen();
        else if(strcmp (tag, "RETURN_TIME") == 0)  load_sys_basic_return_time_screen();
        else if(strcmp (tag, "ADMIN_COUNT") == 0)  load_sys_basic_admin_count_screen();
        else if(strcmp (tag, "WARN_COUNT") == 0)  load_sys_basic_warn_count_screen();        
    }
}

void load_sys_settings_basic_screen(){
    if(scr_basic){
        lv_obj_delete(scr_basic);
        scr_basic = nullptr;
    }

    BaseScreenParts parts = create_base_screen("基础设置");
    scr_basic = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC,&scr_basic);

    lv_obj_add_event_cb(scr_basic, [](lv_event_t *e){
        scr_basic = nullptr;
    },LV_EVENT_DELETE,NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* list = create_list_container(parts.content);

    create_sys_list_btn(list, "1. ", "", "时间设置", sys_basic_event_cb, "TIME");
    create_sys_list_btn(list, "2. ", "", "日期设置", sys_basic_event_cb, "DATE");
    create_sys_list_btn(list, "3. ", "", "音量设置", sys_basic_event_cb, "VOLUME");
    create_sys_list_btn(list, "4. ", "", "语言设置", sys_basic_event_cb, "LANGUAGE");
    create_sys_list_btn(list, "5. ", "", "屏保时间", sys_basic_event_cb, "SCREENSAFE");
    create_sys_list_btn(list, "6. ", "", "机器号设置", sys_basic_event_cb, "MACHINE_ID");
    create_sys_list_btn(list, "7. ", "", "返回主界面时间", sys_basic_event_cb, "RETURN_TIME");
    create_sys_list_btn(list, "8. ", "", "管理员总数", sys_basic_event_cb, "ADMIN_COUNT");
    create_sys_list_btn(list, "9. ", "", "记录警告数", sys_basic_event_cb, "WARN_COUNT");
    
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);
    }

    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_basic);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic);

}

// =================基础设置界面--时间设置（三级）================
static void save_time_to_system(int hour, int min, int sec){
    printf("时间已设置为%02d:%02d:%02d\n", hour, min, sec);
}

static void dialog_confirm_yes_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_indev_wait_release(lv_indev_get_act());
        
        // 从滚轮读取时间并保存
        char buf_hour[8], buf_min[8], buf_sec[8];
        lv_roller_get_selected_str(g_roller_hour, buf_hour, sizeof(buf_hour));
        lv_roller_get_selected_str(g_roller_min, buf_min, sizeof(buf_min));
        lv_roller_get_selected_str(g_roller_sec, buf_sec, sizeof(buf_sec));
        
        int hour = atoi(buf_hour);
        int min = atoi(buf_min);
        int sec = atoi(buf_sec);
        
        save_time_to_system(hour, min, sec);
        
        // 关闭对话框
        if (g_dialog_confirm) {
            lv_obj_del(lv_obj_get_parent(g_dialog_confirm)); // 删除遮罩层
            g_dialog_confirm = nullptr;
        }
        
        // 返回上一级界面
        load_sys_settings_basic_screen();
    }
}

static void dialog_confirm_no_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_indev_wait_release(lv_indev_get_act());
        
        // 关闭对话框，停留在当前界面
        if (g_dialog_confirm) {
            lv_obj_del(lv_obj_get_parent(g_dialog_confirm));
            g_dialog_confirm = nullptr;
        }
    }
}

static void show_time_confirm_dialog(const char* time_str) {
    // 如果已存在对话框，先删除
    if (g_dialog_confirm) {
        lv_obj_del(lv_obj_get_parent(g_dialog_confirm));
        g_dialog_confirm = nullptr;
    }
    
    // 获取当前活动屏幕作为父对象
    lv_obj_t* parent = lv_scr_act();
    
    // 创建遮罩层（半透明背景）
    lv_obj_t* mask = lv_obj_create(parent);
    lv_obj_set_size(mask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_50, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(mask, 0, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    
    // 创建对话框容器
    g_dialog_confirm = lv_obj_create(mask);
    lv_obj_set_size(g_dialog_confirm, 280, 180);
    lv_obj_center(g_dialog_confirm);
    lv_obj_set_style_bg_color(g_dialog_confirm, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_dialog_confirm, 10, 0);
    lv_obj_set_style_shadow_width(g_dialog_confirm, 10, 0);
    lv_obj_set_style_border_width(g_dialog_confirm, 0, 0);
    
    // 设置为列布局
    lv_obj_set_flex_flow(g_dialog_confirm, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_dialog_confirm, LV_FLEX_ALIGN_SPACE_AROUND, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_dialog_confirm, 10, 0);
    
    // 标题
    lv_obj_t* title = lv_label_create(g_dialog_confirm);
    lv_label_set_text(title, "确认修改时间");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    
    // 时间信息
    lv_obj_t* time_label = lv_label_create(g_dialog_confirm);
    char msg[64];
    snprintf(msg, sizeof(msg), "确定将时间修改为\n%s ?", time_str);
    lv_label_set_text(time_label, msg);
    lv_label_set_long_mode(time_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(time_label, LV_PCT(80));
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 按钮容器
    lv_obj_t* btn_panel = lv_obj_create(g_dialog_confirm);
    lv_obj_set_size(btn_panel, LV_PCT(100), 50);
    lv_obj_set_flex_flow(btn_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_panel, LV_FLEX_ALIGN_SPACE_EVENLY, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(btn_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(btn_panel, 0, 0);
    
    // "是"按钮
    g_dialog_btn_yes = create_form_btn (btn_panel, "是", dialog_confirm_yes_cb, NULL);
    lv_obj_set_size(g_dialog_btn_yes, 90, 36);
    
    // "否"按钮
    g_dialog_btn_no = create_form_btn (btn_panel, "否", dialog_confirm_no_cb, NULL);
    lv_obj_set_size(g_dialog_btn_no, 90, 36);  // 覆盖默认的宽度设置

    lv_obj_add_event_cb(g_dialog_btn_yes, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_dialog_btn_yes = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 设置遮罩层的删除回调
    lv_obj_add_event_cb(mask, [](lv_event_t* e) {
        g_dialog_confirm = nullptr;
        g_dialog_btn_yes = nullptr;
        g_dialog_btn_no = nullptr;
    }, LV_EVENT_DELETE, NULL);
    
    // 将对话框按钮加入键盘组
    lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
    if (group) {
        lv_group_add_obj(group, g_dialog_btn_yes);
        lv_group_add_obj(group, g_dialog_btn_no);
        lv_group_focus_obj(g_dialog_btn_yes);
    }
}

static void get_current_time(int &hour, int &min, int &sec){
    time_t now = time(nullptr);
    struct tm *t =localtime(&now);
    hour = t->tm_hour;
    min = t->tm_min;
    sec = t->tm_sec;
}

static void time_confirm_cb(lv_event_t *e){
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);

        if(key == LV_KEY_ESC){
            load_sys_settings_basic_screen();
            lv_indev_wait_release(lv_indev_get_act()); 
            return;
        }

        else if(code == LV_EVENT_CLICKED || key == LV_KEY_ENTER){
            lv_indev_wait_release(lv_indev_get_act()); 

            char buf_hour[8];
            char buf_min[8];
            char buf_sec[8];

            lv_roller_get_selected_str(g_roller_hour, buf_hour, sizeof(buf_hour));
            lv_roller_get_selected_str(g_roller_min,  buf_min,  sizeof(buf_min));
            lv_roller_get_selected_str(g_roller_sec,  buf_sec,  sizeof(buf_sec)); 
            
            int hour = atoi(buf_hour);
            int min = atoi(buf_min);
            int sec = atoi(buf_sec);

            char time_str[20];
            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hour, min, sec);

            show_time_confirm_dialog(time_str);
        }  
    }
}

//时间设置界面
void load_sys_basic_time_settings_screen(){
    if(scr_basic_settime){
        lv_obj_delete(scr_basic_settime);
        scr_basic_settime = nullptr;
    }

    BaseScreenParts parts = create_base_screen("时间设置");
    scr_basic_settime = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_SETTIME,&scr_basic_settime);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_basic_settime, [](lv_event_t * e) {
        scr_basic_settime = nullptr;
    }, LV_EVENT_DELETE, NULL);

    lv_obj_add_event_cb(scr_basic_settime, [](lv_event_t *e) {
        scr_basic_settime = nullptr;
        g_roller_hour = nullptr;
        g_roller_min  = nullptr;
        g_roller_sec  = nullptr;
        g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 获取当前时间
    int current_hour, current_min, current_sec;
    get_current_time(current_hour, current_min, current_sec);

    // 内容区域（由 create_base_screen 提供）
    lv_obj_t *content = parts.content;

    // ---- 时间滚轮面板 ----
    lv_obj_t *time_panel = lv_obj_create(content);
    lv_obj_set_size(time_panel, LV_PCT(90), 120);
    lv_obj_center(time_panel);
    lv_obj_set_flex_flow(time_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(time_panel, 0, 0);
    lv_obj_set_style_pad_all(time_panel, 10, 0);

    // 小时滚轮 (00-23)
    g_roller_hour = lv_roller_create(time_panel);
    lv_roller_set_options(g_roller_hour,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_roller_hour, 3);
    lv_obj_set_width(g_roller_hour, 70);
    lv_roller_set_selected(g_roller_hour, current_hour, LV_ANIM_OFF);

    // 分钟滚轮 (00-59)
    g_roller_min = lv_roller_create(time_panel);
    lv_roller_set_options(g_roller_min,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_roller_min, 3);
    lv_obj_set_width(g_roller_min, 70);
    lv_roller_set_selected(g_roller_min, current_min, LV_ANIM_OFF);

    // 秒滚轮 (00-59)
    g_roller_sec = lv_roller_create(time_panel);
    lv_roller_set_options(g_roller_sec,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_roller_sec, 3);
    lv_obj_set_width(g_roller_sec, 70);
    lv_roller_set_selected(g_roller_sec, current_sec, LV_ANIM_OFF);

    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_pos(btn_cont, 12, 200);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 

    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);


    g_btn_confirm = create_form_btn (btn_cont, "确认", time_confirm_cb,  NULL);
    
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 将所有可聚焦控件加入键盘组
    UiManager::getInstance()->addObjToGroup(g_roller_hour);
    UiManager::getInstance()->addObjToGroup(g_roller_min);
    UiManager::getInstance()->addObjToGroup(g_roller_sec);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点为小时滚轮
    lv_group_focus_obj(g_roller_hour);
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕并销毁其他屏幕
    lv_screen_load(scr_basic_settime);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_settime);  
}

// =================基础设置界面--日期设置（三级）================
static void sys_basic_date_event_cb(lv_event_t *e){
    const char* tag = (const char*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);
  
        if(key == LV_KEY_ESC){
          load_sys_settings_basic_screen();
          lv_indev_wait_release(lv_indev_get_act());
          return;
        }

        else if(key == LV_KEY_DOWN || key == LV_KEY_RIGHT){
          lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
          return;
        }

        else if(key == LV_KEY_UP || key == LV_KEY_LEFT){
          lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
          return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)){
        lv_indev_wait_release(lv_indev_get_act());

        if(strcmp (tag, "DATE_SETTING") == 0) load_sys_basic_date_settings_screen();
        else if(strcmp (tag, "DATE_FORMAT") == 0) load_sys_basic_date_format_screen();
    }
}

void load_sys_basic_date_screen() {

    if (scr_basic_date) {
        lv_obj_delete(scr_basic_date);
        scr_basic_date = nullptr;
    }

    BaseScreenParts parts = create_base_screen("日期设置");
    scr_basic_date = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_DATE, &scr_basic_date);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_basic_date, [](lv_event_t * e) {
        scr_basic_date = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* list = create_list_container(parts.content);

    create_sys_list_btn(list, "1. ", "", "日期设置", sys_basic_date_event_cb, "DATE_SETTING");
    create_sys_list_btn(list, "2. ", "", "日期格式", sys_basic_date_event_cb, "DATE_FORMAT");
    
    uint32_t child_cnt = lv_obj_get_child_cnt(list);

    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);
    }

    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_basic_date);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_date);
}

// =================基础设置界面--日期设置（四级）================
static void get_current_date(int &year, int &month, int &day) {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    year = t->tm_year + 1900;
    month = t->tm_mon + 1;
    day = t->tm_mday;
}

static void save_date_to_system(int year, int month, int day) {
    printf("日期已设置为 %04d-%02d-%02d\n", year, month, day);
    // TODO: 实际调用系统RTC接口设置日期
}

static bool is_valid_date(int year, int month, int day) {
    if (year < 2000 || year > 2099) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    
    // 检查月份天数
    int max_days = 31;
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        max_days = 30;
    } else if (month == 2) {
        // 判断闰年
        bool is_leap = (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
        max_days = is_leap ? 29 : 28;
    }
    
    return day <= max_days;
}

static void date_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);

        if(key == LV_KEY_ESC){
            load_sys_basic_date_screen();
            lv_indev_wait_release(lv_indev_get_act());
            return; 
        }

        else if(code == LV_EVENT_CLICKED || key == LV_KEY_ENTER) {
            lv_indev_wait_release(lv_indev_get_act()); 

            const char* year_str = lv_textarea_get_text(g_ta_year);
            const char* month_str = lv_textarea_get_text(g_ta_month);
            const char* day_str = lv_textarea_get_text(g_ta_day);
            
            // 检查是否为空
            if (strlen(year_str) == 0 || strlen(month_str) == 0 || strlen(day_str) == 0) {
                show_popup_msg("提示", "请填写完整的年月日", nullptr, "确定");
                return;
            }
            
            int year = atoi(year_str);
            int month = atoi(month_str);
            int day = atoi(day_str);
            
            // 验证日期有效性
            if (!is_valid_date(year, month, day)) {
                show_popup_msg("提示", "请输入有效的日期", nullptr, "确定");
                return;
            }

            save_date_to_system(year, month, day);
            
            // 显示成功提示
            show_popup_msg("成功", "日期已保存", nullptr, "确定");
            
            // 返回上一级界面（日期主界面）
            load_sys_basic_date_screen();
            lv_group_focus_obj(scr_basic_date_setting);
        }
    } 
}

static void date_textarea_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_READY) {
        // 输入完成，自动跳转到下一个输入框
        if (lv_event_get_target(e) == g_ta_year) {
            lv_group_focus_obj(g_ta_month);
        } else if (lv_event_get_target(e) == g_ta_month) {
            lv_group_focus_obj(g_ta_day);
        } else if (lv_event_get_target(e) == g_ta_day) {
            // 最后一个输入框，触发确认
            date_confirm_cb(e);
        }
    }
}

void load_sys_basic_date_settings_screen() {
    if (scr_basic_date_setting) {
        lv_obj_delete(scr_basic_date_setting);
        scr_basic_date_setting = nullptr;
    }

    BaseScreenParts parts = create_base_screen("日期设置");
    scr_basic_date_setting = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_SETDATE, &scr_basic_date_setting);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_date_setting, [](lv_event_t *e) {
        scr_basic_date_setting = nullptr;
        g_ta_year = nullptr;
        g_ta_month = nullptr;
        g_ta_day = nullptr;
        g_btn_confirm = nullptr;
        g_btn_cancel = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 获取当前日期
    int current_year, current_month, current_day;
    get_current_date(current_year, current_month, current_day);

    // 内容区域
    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 创建主面板
    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_set_size(panel, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);

     // 设置为垂直布局，三个单元上下排列
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, 5, 0);
    lv_obj_set_style_pad_row(panel, 8, 0);  // 行间距8像素

    // ===== 年份行 =====
    lv_obj_t *row_year = lv_obj_create(panel);
    lv_obj_set_size(row_year, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_year, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_year, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_year, 0, 0);
    lv_obj_set_style_bg_opa(row_year, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(row_year, 10, 0);

    // 年份标签
    lv_obj_t *label_year = lv_label_create(row_year);
    lv_label_set_text(label_year, "年份");
    lv_obj_set_style_text_color(label_year, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(label_year, 50);  // 固定宽度，对齐

    // 年份输入框
    g_ta_year = lv_textarea_create(row_year);
    lv_obj_set_size(g_ta_year, 120, 35);  // 固定宽度120，高度35更紧凑
    lv_textarea_set_placeholder_text(g_ta_year, "2024");
    lv_textarea_set_max_length(g_ta_year, 4);
    lv_textarea_set_one_line(g_ta_year, true);
    lv_textarea_set_accepted_chars(g_ta_year, "0123456789");
    lv_obj_set_style_border_width(g_ta_year, 1, 0);  // 边框1像素更细
    lv_obj_set_style_border_color(g_ta_year, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_ta_year, 3, 0);  // 圆角3像素
    lv_obj_set_style_pad_all(g_ta_year, 5, 0);  // 内边距5像素

    char year_buf[8];
    sprintf(year_buf, "%04d", current_year);
    lv_textarea_set_text(g_ta_year, year_buf);

    // ===== 月份行 =====
    lv_obj_t *row_month = lv_obj_create(panel);
    lv_obj_set_size(row_month, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_month, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_month, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_month, 0, 0);
    lv_obj_set_style_bg_opa(row_month, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(row_month, 10, 0);

    // 月份标签
    lv_obj_t *label_month = lv_label_create(row_month);
    lv_label_set_text(label_month, "月份");
    lv_obj_set_style_text_color(label_month, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(label_month, 50);  // 与年份标签同宽

    // 月份输入框
    g_ta_month = lv_textarea_create(row_month);
    lv_obj_set_size(g_ta_month, 120, 35);
    lv_textarea_set_placeholder_text(g_ta_month, "03");
    lv_textarea_set_max_length(g_ta_month, 2);
    lv_textarea_set_one_line(g_ta_month, true);
    lv_textarea_set_accepted_chars(g_ta_month, "0123456789");
    lv_obj_set_style_border_width(g_ta_month, 1, 0);
    lv_obj_set_style_border_color(g_ta_month, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_ta_month, 3, 0);
    lv_obj_set_style_pad_all(g_ta_month, 5, 0);

    char month_buf[8];
    sprintf(month_buf, "%02d", current_month);
    lv_textarea_set_text(g_ta_month, month_buf);

    // ===== 日期行 =====
    lv_obj_t *row_day = lv_obj_create(panel);
    lv_obj_set_size(row_day, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_day, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_day, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_day, 0, 0);
    lv_obj_set_style_bg_opa(row_day, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(row_day, 10, 0);

    // 日期标签
    lv_obj_t *label_day = lv_label_create(row_day);
    lv_label_set_text(label_day, "日期");
    lv_obj_set_style_text_color(label_day, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(label_day, 50);  // 与年份标签同宽

    // 日期输入框
    g_ta_day = lv_textarea_create(row_day);
    lv_obj_set_size(g_ta_day, 120, 35);
    lv_textarea_set_placeholder_text(g_ta_day, "15");
    lv_textarea_set_max_length(g_ta_day, 2);
    lv_textarea_set_one_line(g_ta_day, true);
    lv_textarea_set_accepted_chars(g_ta_day, "0123456789");
    lv_obj_set_style_border_width(g_ta_day, 1, 0);
    lv_obj_set_style_border_color(g_ta_day, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_ta_day, 3, 0);
    lv_obj_set_style_pad_all(g_ta_day, 5, 0);

    char day_buf[8];
    sprintf(day_buf, "%02d", current_day);
    lv_textarea_set_text(g_ta_day, day_buf);

    // 添加输入框事件
    lv_obj_add_event_cb(g_ta_year, date_textarea_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_ta_month, date_textarea_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_ta_day, date_textarea_cb, LV_EVENT_READY, NULL);

    // 创建按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 70);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    //确认按键
    g_btn_confirm = create_form_btn (btn_cont, "确认", date_confirm_cb,  NULL);

    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 将所有可聚焦控件加入键盘组
    UiManager::getInstance()->addObjToGroup(g_ta_year);
    UiManager::getInstance()->addObjToGroup(g_ta_month);
    UiManager::getInstance()->addObjToGroup(g_ta_day);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点为年份输入框
    lv_group_focus_obj(g_ta_year);
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕并销毁其他屏幕
    lv_screen_load(scr_basic_date_setting);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_date_setting);
}

// =================基础设置界面--日期格式（四级）================
// 日期格式枚举
enum DateFormat {
    FORMAT_YYYY_MM_DD = 0,      // 2024-01-15
    FORMAT_YYYY_MM_DD_SLASH,    // 2024/01/15
    FORMAT_YYYY年MM月DD日,       // 2024年01月15日
    FORMAT_DD_MM_YYYY,          // 15-01-2024
    FORMAT_DD_MM_YYYY_SLASH,    // 15/01/2024
    FORMAT_MM_DD_YYYY,          // 01-15-2024
    FORMAT_MM_DD_YYYY_SLASH,    // 01/15/2024
    FORMAT_COUNT
};

// 格式名称数组（用于显示）
static const char* date_format_names[] = {
    "YYYY-MM-DD",
    "YYYY/MM/DD",
    "YYYY年MM月DD日",
    "DD-MM-YYYY",
    "DD/MM/YYYY",
    "MM-DD-YYYY",
    "MM/DD/YYYY"
};

// 格式示例数组（用于预览）
static const char* date_format_examples[] = {
    "2024-01-15",
    "2024/01/15",
    "2024年01月15日",
    "15-01-2024",
    "15/01/2024",
    "01-15-2024",
    "01/15/2024"
};

// 当前选中的格式索引（可以从系统配置读取）
static int g_current_date_format = FORMAT_YYYY_MM_DD;
static lv_obj_t *g_dropdown_date_format = nullptr;  // 日期格式下拉框

// 保存日期格式到系统
static void save_date_format_to_system(int format_index) {
    g_current_date_format = format_index;
    printf("日期格式已设置为: %s\n", date_format_names[format_index]);
    // TODO: 实际保存到系统配置
}

// 下拉框键盘导航回调
static void date_format_dropdown_keypad_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
    lv_group_t *group = UiManager::getInstance()->getKeypadGroup();
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_UP) {
            if (!lv_dropdown_is_open(dropdown)) {
                lv_group_focus_prev(group);
            }
        } else if (key == LV_KEY_DOWN) {
            if (!lv_dropdown_is_open(dropdown)) {
                lv_group_focus_next(group);
            }
        } else if (key == LV_KEY_ENTER) {
            if (lv_dropdown_is_open(dropdown)) {
                lv_dropdown_close(dropdown);
            } else {
                lv_dropdown_open(dropdown);
            }
        } else if (key == LV_KEY_ESC) {
            if (lv_dropdown_is_open(dropdown)) {
                lv_dropdown_close(dropdown);
            } else {
                load_sys_basic_date_screen(); 
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    }
}

// 下拉框值改变回调
static void date_format_dropdown_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t *preview_label = (lv_obj_t*)lv_event_get_user_data(e);
        
        // 获取当前选中的索引
        int selected = lv_dropdown_get_selected(dropdown);
        
        // 更新预览标签
        if (preview_label) {
            char preview_text[64];
            snprintf(preview_text, sizeof(preview_text), "预览: %s", date_format_examples[selected]);
            lv_label_set_text(preview_label, preview_text);
        }
    }
}

// 确认按钮回调
static void format_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);

        if(key == LV_KEY_ESC){
            load_sys_basic_date_screen(); 
            lv_indev_wait_release(lv_indev_get_act());
            return; 
        } else if(code == LV_EVENT_CLICKED || key == LV_KEY_ENTER) {
            lv_indev_wait_release(lv_indev_get_act()); 

            // 获取选中的格式索引
            int selected = lv_dropdown_get_selected(g_dropdown_date_format);
            
            // 保存到系统
            save_date_format_to_system(selected);
            
            // 显示成功提示（使用现有的弹窗）
            show_popup_msg("成功", "日期格式已保存", nullptr, "确定");
            
            // 返回上一级界面（日期主界面）
            load_sys_basic_date_screen();
        }
    }
}

void load_sys_basic_date_format_screen() {
    // 如果屏幕已存在，先删除
    if (scr_basic_date_format) {
        lv_obj_delete(scr_basic_date_format);
        scr_basic_date_format = nullptr;
    }

    BaseScreenParts parts = create_base_screen("日期格式");
    scr_basic_date_format = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_DATE_FORMAT, &scr_basic_date_format);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_date_format, [](lv_event_t *e) {
        scr_basic_date_format = nullptr;
        g_dropdown_date_format = nullptr;
        g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 内容区域
    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 15, 0);

    // ===== 标题提示 =====
    lv_obj_t *label_hint = lv_label_create(content);
    lv_label_set_text(label_hint, "请选择日期显示格式:");
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(label_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label_hint, LV_PCT(100));

    // ===== 创建日期格式下拉框 =====
    // 准备下拉框选项数据
    std::vector<std::pair<int, std::string>> date_format_options;
    for (int i = 0; i < FORMAT_COUNT; i++) {
        date_format_options.push_back({i, date_format_names[i]});
    }
    
    // 使用下拉框函数创建日期格式选择
    g_dropdown_date_format = create_form_dropdown(
        content,                    // 父容器
        "格式",                     // 标签文本
        date_format_options,        // 选项数据
        g_current_date_format       // 默认选中当前格式
    );
    
    // 为下拉框添加键盘导航回调
    lv_obj_add_event_cb(g_dropdown_date_format, date_format_dropdown_keypad_cb, LV_EVENT_KEY, NULL);

    // 创建预览标签
    lv_obj_t *preview_label = lv_label_create(content);
    char preview_text[64];
    snprintf(preview_text, sizeof(preview_text), "预览: %s", date_format_examples[g_current_date_format]);
    lv_label_set_text(preview_label, preview_text);
    lv_obj_set_style_text_color(preview_label, lv_color_hex(0x2196F3), 0);  // 蓝色文字
    lv_obj_set_style_text_font(preview_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(preview_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(preview_label, LV_PCT(100));

    // 为下拉框添加值改变事件，更新预览
    lv_obj_add_event_cb(g_dropdown_date_format, date_format_dropdown_cb, LV_EVENT_VALUE_CHANGED, preview_label);

    // ===== 创建按钮容器 =====
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 创建确认按钮
    g_btn_confirm = create_form_btn(btn_cont, "确认", format_confirm_cb, NULL);
    lv_obj_add_flag(g_btn_confirm, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 为按钮添加键盘导航回调
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_group_t *group = UiManager::getInstance()->getKeypadGroup();
        
        if (code == LV_EVENT_KEY) {
            uint32_t key = lv_event_get_key(e);
            
            if (key == LV_KEY_UP) {
                lv_group_focus_prev(group);
            } else if (key == LV_KEY_DOWN) {
                lv_group_focus_next(group);
            } else if (key == LV_KEY_ESC) {
                load_sys_basic_date_screen();
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    }, LV_EVENT_KEY, NULL);

    // ===== 加入键盘组 =====
    UiManager::getInstance()->addObjToGroup(g_dropdown_date_format);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置编辑模式
    lv_group_set_editing(UiManager::getInstance()->getKeypadGroup(), false);
    
    // 为下拉框添加标志，使其可聚焦
    lv_obj_add_flag(g_dropdown_date_format, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // 设置初始焦点到下拉框
    lv_group_focus_obj(g_dropdown_date_format);

    // 加载屏幕
    lv_screen_load(scr_basic_date_format);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_date_format);
}

// =================基础设置-音量设置================
// 保存音量设置到系统
static void save_volume_to_system(int volume, bool muted) {
    printf("音量已设置为: %d, 静音状态: %s\n", volume, muted ? "开启" : "关闭");
    g_current_volume = volume;
    g_is_muted = muted;
    // TODO: 实际调用系统接口设置音量
}

// 滑块事件回调
static void volume_slider_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        
        // 更新数值显示
        char buf[8];
        sprintf(buf, "%d%%", value);
        lv_label_set_text(g_label_volume_value, buf);
        
        // 如果音量大于0，自动关闭静音
        if (value > 0 && g_is_muted) {
            lv_obj_clear_state(g_switch_mute, LV_STATE_CHECKED);
            g_is_muted = false;
        }
        // 如果音量为0，自动开启静音
        else if (value == 0 && !g_is_muted) {
            lv_obj_add_state(g_switch_mute, LV_STATE_CHECKED);
            g_is_muted = true;
        }
    }
}

// 静音开关事件回调
static void mute_switch_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
        bool is_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        
        g_is_muted = is_checked;
        
        // 如果开启静音，将滑块设置为0
        if (is_checked) {
            lv_slider_set_value(g_slider_volume, 0, LV_ANIM_ON);
            lv_label_set_text(g_label_volume_value, "0%");
        }
        // 如果关闭静音且当前音量为0，恢复到之前保存的音量
        else if (g_current_volume > 0) {
            lv_slider_set_value(g_slider_volume, g_current_volume, LV_ANIM_ON);
            char buf[8];
            sprintf(buf, "%d%%", g_current_volume);
            lv_label_set_text(g_label_volume_value, buf);
        }
    }
}

// 确认按钮回调
static void volume_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        int volume = lv_slider_get_value(g_slider_volume);
        bool muted = lv_obj_has_state(g_switch_mute, LV_STATE_CHECKED);
        
        save_volume_to_system(volume, muted);
        
        // 显示成功提示
        show_popup_msg("成功", "音量设置已保存", nullptr, "确定");
        
        // 返回基础设置界面
        load_sys_settings_basic_screen();
    }
}

// 加载音量设置界面
void load_sys_basic_volume_settings_screen() {
    if (scr_basic_volume) {
        lv_obj_delete(scr_basic_volume);
        scr_basic_volume = nullptr;
    }

    BaseScreenParts parts = create_base_screen("音量设置");
    scr_basic_volume = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_VOLUME, &scr_basic_volume);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_volume, [](lv_event_t *e) {
        scr_basic_volume = nullptr;
        g_slider_volume = nullptr;
        g_label_volume_value = nullptr;
        g_switch_mute = nullptr;
        g_btn_confirm = nullptr;
        g_btn_cancel = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 内容区域
    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 5, 0);
    lv_obj_set_style_pad_row(content, 10, 0);

    // ===== 音量标题 =====
    lv_obj_t *icon_label = lv_label_create(content);
    lv_label_set_text(icon_label, "当前音量");
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_16, 0);  
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x2196F3), 0);

    // ===== 音量数值显示 =====
    g_label_volume_value = lv_label_create(content);
    char buf[16];
    sprintf(buf, "%d%%", g_current_volume);
    lv_label_set_text(g_label_volume_value, buf);
    lv_obj_set_style_text_font(icon_label,&lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_label_volume_value, lv_color_hex(0xFFFFFF), 0);
    
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 10, 20);
    lv_obj_align_to(g_label_volume_value, icon_label, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    // ===== 音量滑块 =====
    lv_obj_t *slider_cont = lv_obj_create(content);
    lv_obj_set_size(slider_cont, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(slider_cont, 0, 0);
    lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slider_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(slider_cont, 8, 0);

    // 减号按钮
    lv_obj_t *btn_minus = lv_btn_create(slider_cont);
    lv_obj_set_size(btn_minus, 28, 28);
    lv_obj_set_style_bg_color(btn_minus, lv_color_hex(0x2196F3), 0);
    lv_obj_t *label_minus = lv_label_create(btn_minus);
    lv_label_set_text(label_minus, "-");
    lv_obj_center(label_minus);

    // 滑块
    g_slider_volume = lv_slider_create(slider_cont);
    lv_obj_set_width(g_slider_volume, 80);
    lv_slider_set_range(g_slider_volume, 0, 100);
    lv_slider_set_value(g_slider_volume, g_current_volume, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_slider_volume, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(g_slider_volume, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);

    // 加号按钮
    lv_obj_t *btn_plus = lv_btn_create(slider_cont);
    lv_obj_set_size(btn_plus, 28, 28);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x2196F3), 0);
    lv_obj_t *label_plus = lv_label_create(btn_plus);
    lv_label_set_text(label_plus, "+");
    lv_obj_center(label_plus);

    // 为加减按钮添加点击事件
    lv_obj_add_event_cb(btn_minus, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            int val = lv_slider_get_value(g_slider_volume);
            val = val > 0 ? val - 10 : 0;
            lv_slider_set_value(g_slider_volume, val, LV_ANIM_ON);
            volume_slider_cb(e);
        }
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(btn_plus, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            int val = lv_slider_get_value(g_slider_volume);
            val = val < 100 ? val + 10 : 100;
            lv_slider_set_value(g_slider_volume, val, LV_ANIM_ON);
            volume_slider_cb(e);
        }
    }, LV_EVENT_CLICKED, NULL);

    // ===== 静音开关 =====
    lv_obj_t *mute_cont = lv_obj_create(content);
    lv_obj_set_size(mute_cont, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(mute_cont, 0, 0);
    lv_obj_set_style_bg_opa(mute_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(mute_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mute_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(mute_cont, 15, 0);

    lv_obj_t *mute_label = lv_label_create(mute_cont);
    lv_label_set_text(mute_label, "静音模式");
    lv_obj_set_style_text_color(mute_label, lv_color_hex(0xFFFFFF), 0);

    g_switch_mute = lv_switch_create(mute_cont);
    if (g_is_muted) {
        lv_obj_add_state(g_switch_mute, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(g_switch_mute, mute_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ===== 按钮容器 =====
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 70);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", volume_confirm_cb, NULL);
  
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm= nullptr;
    }, LV_EVENT_DELETE, NULL);

    // ===== 加入键盘组 =====
    UiManager::getInstance()->addObjToGroup(g_slider_volume);
    UiManager::getInstance()->addObjToGroup(g_switch_mute);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);
    UiManager::getInstance()->addObjToGroup(btn_minus);
    UiManager::getInstance()->addObjToGroup(btn_plus);

    // 设置初始焦点
    lv_group_focus_obj(g_slider_volume);
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕
    lv_screen_load(scr_basic_volume);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_volume);
}

// =================基础设置-语言设置================
// 语言枚举
enum LanguageType {
    LANGUAGE_CHINESE = 0,    // 中文
    LANGUAGE_ENGLISH = 1,    // 英文
    LANGUAGE_COUNT
};

// 语言名称数组（用于显示）
static const char* language_names[] = {
    "中文 (简体)",
    "English"
};

// 语言代码数组（用于系统配置）
static const char* language_codes[] = {
    "zh_CN",
    "en_US"
};

// 当前语言（可以从系统配置读取）
static int g_current_language = LANGUAGE_CHINESE;  // 默认中文

// 保存语言设置到系统
static void save_language_to_system(int lang_index) {
    g_current_language = lang_index;
    printf("语言已设置为: %s (%s)\n", 
           language_names[lang_index], 
           language_codes[lang_index]);
    // TODO: 实际调用系统接口设置语言
    // 例如：set_system_language(language_codes[lang_index]);
}

// 下拉框值改变回调
static void language_dropdown_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t *preview_label = (lv_obj_t*)lv_event_get_user_data(e);
        
        // 获取当前选中的索引
        int selected = lv_dropdown_get_selected(dropdown);
        
        // 更新预览标签
        if (preview_label) {
            if (selected == LANGUAGE_CHINESE) {
                lv_label_set_text(preview_label, "示例: 设置 | 返回 | 确认");
            } else {
                lv_label_set_text(preview_label, "Example: Settings | Back | OK");
            }
        }
    }
}

// 确认按钮回调
static void language_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        // 获取选中的语言索引
        int selected = lv_dropdown_get_selected(g_dropdown_language);
        
        // 保存语言设置
        save_language_to_system(selected);
        
        // 显示成功提示
        if (selected == LANGUAGE_CHINESE) {
            show_popup_msg("成功", "语言设置已保存，重启后生效", nullptr, "确定");
        } else {
            show_popup_msg("Success", "Language setting saved, will take effect after restart", nullptr, "OK");
        }
        
        // 返回基础设置界面
        load_sys_settings_basic_screen();
    }
}

// 加载语言设置界面（使用下拉框版本）
void load_sys_basic_language_settings_screen() {
    // 如果屏幕已存在，先删除
    if (scr_basic_language) {
        lv_obj_delete(scr_basic_language);
        scr_basic_language = nullptr;
    }

    BaseScreenParts parts = create_base_screen("语言设置");
    scr_basic_language = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_LANGUAGE, &scr_basic_language);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_language, [](lv_event_t *e) {
        scr_basic_language = nullptr;
        g_dropdown_language = nullptr;
        g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 内容区域
    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 15, 0);

    // ===== 提示文字 =====
    lv_obj_t *hint_label = lv_label_create(content);
    lv_label_set_text(hint_label, "选择语言 / Select Language");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);

    // ===== 创建语言下拉框 =====
    // 准备下拉框选项数据
    std::vector<std::pair<int, std::string>> language_options;
    for (int i = 0; i < LANGUAGE_COUNT; i++) {
        language_options.push_back({i, language_names[i]});
    }
    
    // 使用下拉框函数创建语言选择
    g_dropdown_language = create_form_dropdown(
        content,                    // 父容器
        "语言",                     // 标签文本
        language_options,           // 选项数据
        g_current_language          // 默认选中当前语言
    );
    
    // 为下拉框添加值改变事件
    lv_obj_t *preview_label = lv_label_create(content);
    if (g_current_language == LANGUAGE_CHINESE) {
        lv_label_set_text(preview_label, "示例: 设置 | 返回 | 确认");
    } else {
        lv_label_set_text(preview_label, "Example: Settings | Back | OK");
    }
    lv_obj_set_style_text_color(preview_label, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(preview_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(preview_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(preview_label, LV_PCT(100));
    
    // 为下拉框添加事件回调
    lv_obj_add_event_cb(g_dropdown_language, language_dropdown_cb, LV_EVENT_VALUE_CHANGED, preview_label);

    // ===== 按钮容器 =====
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn(btn_cont, "确认", language_confirm_cb, NULL);
  
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // ===== 加入键盘组 =====
    UiManager::getInstance()->addObjToGroup(g_dropdown_language);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置编辑模式
    lv_group_set_editing(UiManager::getInstance()->getKeypadGroup(), true);

    // 设置初始焦点到下拉框
    lv_group_focus_obj(g_dropdown_language);

    // 加载屏幕
    lv_screen_load(scr_basic_language);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_language);
}

// =================基础设置-屏保时间设置================
static const int screensafe_time_options[] = {0, 1, 3, 5, 10, 15, 30, 60, 120, 180, 300};
static const char* screensafe_time_names[] = {
    "从不", 
    "1分钟", 
    "3分钟", 
    "5分钟", 
    "10分钟", 
    "15分钟", 
    "30分钟", 
    "1小时", 
    "2小时", 
    "3小时", 
    "5小时"
};
static const int SCREENSAFE_TIME_COUNT = sizeof(screensafe_time_options) / sizeof(screensafe_time_options[0]);

static lv_obj_t *g_dropdown_screensafe = nullptr;  // 屏保时间下拉框
static lv_obj_t *g_switch_screensafe_enable = nullptr;  // 屏保启用开关
static lv_obj_t *g_label_preview = nullptr;  // 预览标签

// 当前屏保设置
static bool g_screensafe_enabled = true;
static int g_screensafe_time_index = 5;  // 默认30分钟

// 保存屏保设置到系统
static void save_screensafe_to_system(bool enabled, int time_index) {
    g_screensafe_enabled = enabled;
    g_screensafe_time_index = time_index;
    int time_minutes = screensafe_time_options[time_index];
    printf("屏保设置已保存: %s, 时间: %s (%d分钟)\n", 
           enabled ? "启用" : "禁用", 
           screensafe_time_names[time_index],
           time_minutes);
    // TODO: 实际调用系统接口设置屏保
}

// 开关事件回调
static void screensafe_switch_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
        bool is_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        
        // 更新下拉框状态
        if (g_dropdown_screensafe) {
            if (is_checked) {
                lv_obj_clear_state(g_dropdown_screensafe, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(g_dropdown_screensafe, LV_STATE_DISABLED);
            }
        }
        
        // 更新预览
        if (g_label_preview) {
            if (is_checked) {
                int selected = lv_dropdown_get_selected(g_dropdown_screensafe);
                char buf[64];
                if (selected == 0) {
                    snprintf(buf, sizeof(buf), "屏保已启用，不会自动关闭屏幕");
                } else {
                    snprintf(buf, sizeof(buf), "无操作 %s 后关闭屏幕", screensafe_time_names[selected]);
                }
                lv_label_set_text(g_label_preview, buf);
            } else {
                lv_label_set_text(g_label_preview, "屏保已禁用");
            }
        }
    }
}

// 下拉框键盘导航回调
static void screensafe_dropdown_keypad_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
    lv_group_t *group = UiManager::getInstance()->getKeypadGroup();
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
            // 当下拉列表未展开时，上下键用于焦点移动
            if (!lv_dropdown_is_open(dropdown)) {
                if (key == LV_KEY_UP) {
                    lv_group_focus_prev(group);
                } else if (key == LV_KEY_DOWN) {
                    lv_group_focus_next(group);
                }
            }    
        } 
        
        else if (key == LV_KEY_ENTER) {
            // 回车键切换下拉列表展开/收起
            if (lv_dropdown_is_open(dropdown)) {
                lv_dropdown_close(dropdown);
            } else {
                lv_dropdown_open(dropdown);
            }
        } else if (key == LV_KEY_ESC) {
            // ESC键关闭下拉列表
            if (lv_dropdown_is_open(dropdown)) {
                lv_dropdown_close(dropdown);
            } else {
                // 如果没有展开，返回上级界面
                load_sys_settings_basic_screen();
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    }
}

// 下拉框值改变回调
static void screensafe_dropdown_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
        int selected = lv_dropdown_get_selected(dropdown);
        
        // 更新预览
        if (g_label_preview && g_screensafe_enabled) {
            char buf[64];
            if (selected == 0) {
                snprintf(buf, sizeof(buf), "屏保已启用，不会自动关闭屏幕");
            } else {
                snprintf(buf, sizeof(buf), "无操作 %s 后关闭屏幕", screensafe_time_names[selected]);
            }
            lv_label_set_text(g_label_preview, buf);
        }
    }
}

// 确认按钮回调
static void screensafe_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        bool enabled = lv_obj_has_state(g_switch_screensafe_enable, LV_STATE_CHECKED);
        int time_index = lv_dropdown_get_selected(g_dropdown_screensafe);
        
        save_screensafe_to_system(enabled, time_index);
        
        show_popup_msg("成功", "屏保设置已保存", nullptr, "确定");
        
        load_sys_settings_basic_screen();
    }
}

void load_sys_basic_screensafe_settings_screen() {
    if (scr_basic_screensafe) {
        lv_obj_delete(scr_basic_screensafe);
        scr_basic_screensafe = nullptr;
    }

    BaseScreenParts parts = create_base_screen("屏保设置");
    scr_basic_screensafe = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_SCREENSAFE, &scr_basic_screensafe);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_screensafe, [](lv_event_t *e) {
        scr_basic_screensafe = nullptr;
        g_dropdown_screensafe = nullptr;
        g_switch_screensafe_enable = nullptr;
        g_label_preview = nullptr;
        g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 15, 0);

    // ===== 启用/禁用开关 =====
    lv_obj_t *switch_cont = lv_obj_create(content);
    lv_obj_set_size(switch_cont, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(switch_cont, 0, 0);
    lv_obj_set_style_bg_opa(switch_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(switch_cont, 0, 0);

    lv_obj_t *switch_label = lv_label_create(switch_cont);
    lv_label_set_text(switch_label, "启用屏保");
    lv_obj_set_style_text_color(switch_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(switch_label, &lv_font_montserrat_16, 0);

    g_switch_screensafe_enable = lv_switch_create(switch_cont);
    lv_obj_set_size(g_switch_screensafe_enable, 40, 20);
    if (g_screensafe_enabled) {
        lv_obj_add_state(g_switch_screensafe_enable, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(g_switch_screensafe_enable, screensafe_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ===== 屏保时间下拉框 =====
    // 准备下拉框选项数据
    std::vector<std::pair<int, std::string>> screensafe_options;
    for (int i = 0; i < SCREENSAFE_TIME_COUNT; i++) {
        screensafe_options.push_back({i, screensafe_time_names[i]});
    }
    
    // 使用下拉框函数创建屏保时间选择
    g_dropdown_screensafe = create_form_dropdown(
        content,                    // 父容器
        "屏保时间",                 // 标签文本
        screensafe_options,         // 选项数据
        g_screensafe_time_index     // 默认选中索引
    );
    
    // 为下拉框添加键盘导航回调
    lv_obj_add_event_cb(g_dropdown_screensafe, screensafe_dropdown_keypad_cb, LV_EVENT_KEY, NULL);
    
    // 为下拉框添加值改变事件
    lv_obj_add_event_cb(g_dropdown_screensafe, screensafe_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 根据开关状态设置下拉框状态
    if (!g_screensafe_enabled) {
        lv_obj_add_state(g_dropdown_screensafe, LV_STATE_DISABLED);
    }

    // ===== 预览标签 =====
    g_label_preview = lv_label_create(content);
    if (g_screensafe_enabled) {
        char buf[64];
        if (g_screensafe_time_index == 0) {
            snprintf(buf, sizeof(buf), "屏保已启用，不会自动关闭屏幕");
        } else {
            snprintf(buf, sizeof(buf), "无操作 %s 后关闭屏幕", screensafe_time_names[g_screensafe_time_index]);
        }
        lv_label_set_text(g_label_preview, buf);
    } else {
        lv_label_set_text(g_label_preview, "屏保已禁用");
    }
    lv_obj_set_style_text_color(g_label_preview, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(g_label_preview, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(g_label_preview, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_label_preview, LV_PCT(100));

    // ===== 按钮容器 =====
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn(btn_cont, "确认", screensafe_confirm_cb, NULL);
    lv_obj_add_flag(g_btn_confirm, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 为按钮添加键盘导航回调
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_group_t *group = UiManager::getInstance()->getKeypadGroup();
        
        if (code == LV_EVENT_KEY) {
            uint32_t key = lv_event_get_key(e);
            
            if (key == LV_KEY_UP) {
                lv_group_focus_prev(group);
            } else if (key == LV_KEY_DOWN) {
                lv_group_focus_next(group);
            } else if (key == LV_KEY_ESC) {
                load_sys_settings_basic_screen();
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    }, LV_EVENT_KEY, NULL);

    // ===== 加入键盘组 =====
    UiManager::getInstance()->addObjToGroup(g_switch_screensafe_enable);
    UiManager::getInstance()->addObjToGroup(g_dropdown_screensafe);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置编辑模式
    lv_group_set_editing(UiManager::getInstance()->getKeypadGroup(), true);
    
    // 为下拉框和开关添加标志，使其可聚焦
    lv_obj_add_flag(g_dropdown_screensafe, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(g_switch_screensafe_enable, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // 设置初始焦点到开关
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕
    lv_screen_load(scr_basic_screensafe);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_screensafe);
}

// =================基础设置-机器号设置（三级）================
// 当前机器号
static char g_machine_id[32] = "MACHINE_001";

// 保存机器号到系统
static void save_machine_id_to_system(const char* machine_id) {
    strncpy(g_machine_id, machine_id, sizeof(g_machine_id) - 1);
    g_machine_id[sizeof(g_machine_id) - 1] = '\0';
    printf("机器号已设置为: %s\n", g_machine_id);
    // TODO: 实际保存到系统配置
}

// 确认按钮回调
static void machine_id_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        const char* machine_id = lv_textarea_get_text(g_ta_machine_id);
        
        if (strlen(machine_id) == 0) {
            show_popup_msg("提示", "请输入机器号", nullptr, "确定");
            return;
        }
        
        save_machine_id_to_system(machine_id);
        show_popup_msg("成功", "机器号已保存", nullptr, "确定");
        load_sys_settings_basic_screen();
    }
}

void load_sys_basic_machine_id_screen() {
    if (scr_basic_machine_id) {
        lv_obj_delete(scr_basic_machine_id);
        scr_basic_machine_id = nullptr;
    }

    BaseScreenParts parts = create_base_screen("机器号设置");
    scr_basic_machine_id = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_MACHINE_ID, &scr_basic_machine_id);

    lv_obj_add_event_cb(scr_basic_machine_id, [](lv_event_t *e) {
        scr_basic_machine_id = nullptr;
        g_ta_machine_id = nullptr;
        g_btn_confirm = nullptr;
        g_btn_cancel = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;

    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 5, 0);
    lv_obj_set_style_pad_row(content, 8, 0);

    // 提示标签
    lv_obj_t *hint_label = lv_label_create(content);
    lv_label_set_text(hint_label, "请输入机器号（最多16位）");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);

    // 输入框容器
    lv_obj_t *input_cont = lv_obj_create(content);
    lv_obj_set_size(input_cont, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(input_cont, 0, 0);
    lv_obj_set_style_bg_opa(input_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(input_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(input_cont, 10, 0);

    // 机器号标签
    lv_obj_t *label_prefix = lv_label_create(input_cont);
    lv_label_set_text(label_prefix, "机器号:");
    lv_obj_set_style_text_color(label_prefix, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_prefix, &lv_font_montserrat_16, 0);

    // 输入框
    g_ta_machine_id = lv_textarea_create(input_cont);
    lv_obj_set_size(g_ta_machine_id, 120, 45);
    lv_textarea_set_placeholder_text(g_ta_machine_id, "MACHINE_001");
    lv_textarea_set_max_length(g_ta_machine_id, 16);
    lv_textarea_set_one_line(g_ta_machine_id, true);
    lv_textarea_set_accepted_chars(g_ta_machine_id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-");
    lv_obj_set_style_border_width(g_ta_machine_id, 1, 0);
    lv_obj_set_style_border_color(g_ta_machine_id, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_ta_machine_id, 5, 0);
    lv_obj_set_style_pad_all(g_ta_machine_id, 8, 0);
    lv_obj_set_style_bg_color(g_ta_machine_id, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(g_ta_machine_id, lv_color_hex(0x2196F3), 0);
    
    lv_textarea_set_text(g_ta_machine_id, g_machine_id);

    // 添加键盘完成事件
    lv_obj_add_event_cb(g_ta_machine_id, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_READY) {
            machine_id_confirm_cb(e);
        }
    }, LV_EVENT_READY, NULL);

    // 当前机器号显示
    lv_obj_t *current_label = lv_label_create(content);
    char current_buf[64];
    snprintf(current_buf, sizeof(current_buf), "当前机器号: %s", g_machine_id);
    lv_label_set_text(current_label, current_buf);
    lv_obj_set_style_text_color(current_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_14, 0);

    // 格式提示
    lv_obj_t *format_label = lv_label_create(content);
    lv_label_set_text(format_label, "可使用字母、数字、下划线、连字符");
    lv_obj_set_style_text_color(format_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(format_label, &lv_font_montserrat_16, 0);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

     // 确认按钮
    g_btn_confirm= create_form_btn (btn_cont, "确认", machine_id_confirm_cb, nullptr);
  
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm= nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_ta_machine_id);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕
    lv_screen_load(scr_basic_machine_id);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_machine_id);
}


// =================基础设置-返回主界面时间设置（三级）================
// 返回时间选项（单位：秒）
static const int return_time_options[] = {0, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600};
static const char* return_time_names[] = {
    "从不返回",
    "5秒",
    "10秒",
    "15秒",
    "30秒",
    "1分钟",
    "2分钟",
    "5分钟",
    "10分钟",
    "30分钟",
    "1小时"
};
static const int RETURN_TIME_COUNT = sizeof(return_time_options) / sizeof(return_time_options[0]);

static int g_return_time_index = 3;  // 默认15秒

// 保存返回时间设置
static void save_return_time_to_system(int time_index) {
    g_return_time_index = time_index;
    printf("返回主界面时间已设置为: %s (%d秒)\n", 
           return_time_names[time_index], 
           return_time_options[time_index]);
    // TODO: 实际保存到系统配置
}

// 确认按钮回调
static void return_time_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        int selected = lv_roller_get_selected(g_roller_return_time);
        save_return_time_to_system(selected);
        
        show_popup_msg("成功", "返回时间已保存", nullptr, "确定");
        load_sys_settings_basic_screen();
    }
}

// 滚轮值改变回调（用于预览）
static void return_time_roller_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *roller = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t *preview_label = (lv_obj_t*)lv_event_get_user_data(e);
        
        if (preview_label) {
            int selected = lv_roller_get_selected(roller);
            char buf[64];
            if (return_time_options[selected] == 0) {
                snprintf(buf, sizeof(buf), "不会自动返回主界面");
            } else {
                snprintf(buf, sizeof(buf), "无操作 %s 后自动返回主界面", 
                         return_time_names[selected]);
            }
            lv_label_set_text(preview_label, buf);
        }
    }
}

void load_sys_basic_return_time_screen() {
    if (scr_basic_return_time) {
        lv_obj_delete(scr_basic_return_time);
        scr_basic_return_time = nullptr;
    }

    BaseScreenParts parts = create_base_screen("返回时间");
    scr_basic_return_time = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_RETURN_TIME, &scr_basic_return_time);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_return_time, [](lv_event_t *e) {
        scr_basic_return_time = nullptr;
        g_roller_return_time = nullptr;
        g_btn_confirm = nullptr;
        g_btn_cancel = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 5, 0);
    lv_obj_set_style_pad_row(content, 8, 0);

    // 标题提示
    lv_obj_t *title_cont = lv_obj_create(content);
    lv_obj_set_size(title_cont, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(title_cont, 10, 0);

    // 提示标签
    lv_obj_t *hint_label = lv_label_create(title_cont);
    lv_label_set_text(hint_label, "无操作后自动返回主界面");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);

    // 当前设置显示
    lv_obj_t *current_label = lv_label_create(content);
    char current_buf[64];
    if (return_time_options[g_return_time_index] == 0) {
        snprintf(current_buf, sizeof(current_buf), "当前设置: 从不返回");
    } else {
        snprintf(current_buf, sizeof(current_buf), "当前设置: %s", 
                 return_time_names[g_return_time_index]);
    }
    lv_label_set_text(current_label, current_buf);
    lv_obj_set_style_text_color(current_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_14, 0);

    // 滚轮容器（带边框效果）
    lv_obj_t *roller_cont = lv_obj_create(content);
    lv_obj_set_size(roller_cont, LV_PCT(100), 120);
    lv_obj_set_style_bg_opa(roller_cont, LV_OPA_TRANSP, 0);  // 背景透明
    lv_obj_set_style_border_width(roller_cont, 0, 0);        // 无边框
    lv_obj_set_style_pad_all(roller_cont, 0, 0);             // 无内边距
    lv_obj_center(roller_cont);

    // 创建滚轮
    g_roller_return_time = lv_roller_create(roller_cont);
    lv_obj_center(g_roller_return_time);
    lv_obj_set_style_bg_opa(g_roller_return_time, LV_OPA_TRANSP, 0);
    
    // 构建选项字符串
    char options[512] = "";
    for (int i = 0; i < RETURN_TIME_COUNT; i++) {
        strcat(options, return_time_names[i]);
        if (i < RETURN_TIME_COUNT - 1) {
            strcat(options, "\n");
        }
    }
    
    lv_roller_set_options(g_roller_return_time, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_roller_return_time, 5);
    lv_obj_set_width(g_roller_return_time, 180);
    lv_obj_set_style_text_font(g_roller_return_time, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(g_roller_return_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_roller_return_time, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(g_roller_return_time, lv_color_hex(0x333333), 0);
    
    lv_roller_set_selected(g_roller_return_time, g_return_time_index, LV_ANIM_OFF);

    // 预览标签
    lv_obj_t *preview_label = lv_label_create(content);
    if (return_time_options[g_return_time_index] == 0) {
        lv_label_set_text(preview_label, "不会自动返回主界面");
    } else {
        char preview_buf[64];
        snprintf(preview_buf, sizeof(preview_buf), "预览: 无操作 %s 后自动返回", 
                 return_time_names[g_return_time_index]);
        lv_label_set_text(preview_label, preview_buf);
    }
    lv_obj_set_style_text_color(preview_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(preview_label, &lv_font_montserrat_14, 0);

    // 为滚轮添加值改变事件
    lv_obj_add_event_cb(g_roller_return_time, return_time_roller_cb, 
                        LV_EVENT_VALUE_CHANGED, preview_label);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 70);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", return_time_confirm_cb, NULL);
 
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_roller_return_time);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置编辑模式（滚轮需要）
    lv_group_set_editing(UiManager::getInstance()->getKeypadGroup(), true);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕
    lv_screen_load(scr_basic_return_time);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_return_time);
}

// =================基础设置-管理员总数设置（三级）================
// 保存管理员总数到系统
static void save_admin_count_to_system(int count) {
    g_admin_count = count;
    printf("管理员总数已设置为: %d\n", g_admin_count);
    // TODO: 实际保存到系统配置，例如写入配置文件或数据库
}

// 确认按钮回调
static void admin_count_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        const char* count_str = lv_textarea_get_text(g_ta_admin_count);
        
        if (strlen(count_str) == 0) {
            show_popup_msg("提示", "请输入管理员总数", nullptr, "确定");
            return;
        }
        
        int count = atoi(count_str);
        
        // 验证输入范围（根据实际需求调整）
        if (count < 1 || count > 100) {
            show_popup_msg("提示", "管理员总数范围: 1-100", nullptr, "确定");
            return;
        }
        
        save_admin_count_to_system(count);
        show_popup_msg("成功", "管理员总数已保存", nullptr, "确定");
        load_sys_settings_basic_screen();
    }
}

void load_sys_basic_admin_count_screen() {
    if (scr_basic_admin_count) {
        lv_obj_delete(scr_basic_admin_count);
        scr_basic_admin_count = nullptr;
    }

    BaseScreenParts parts = create_base_screen("管理员总数");
    scr_basic_admin_count = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_ADMIN_COUNT, &scr_basic_admin_count);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_admin_count, [](lv_event_t *e) {
        scr_basic_admin_count = nullptr;
        g_ta_admin_count = nullptr;
        g_btn_confirm = nullptr;
        g_btn_cancel = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 5, 0);
    lv_obj_set_style_pad_row(content, 8, 0);

    // 提示标签
    lv_obj_t *hint_label = lv_label_create(content);
    lv_label_set_text(hint_label, "设置系统管理员最大数量");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);

    // 输入框容器
    lv_obj_t *input_cont = lv_obj_create(content);
    lv_obj_set_size(input_cont, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(input_cont, 0, 0);
    lv_obj_set_style_bg_opa(input_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(input_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(input_cont, 15, 0);

    // 数量标签
    lv_obj_t *label_prefix = lv_label_create(input_cont);
    lv_label_set_text(label_prefix, "数量:");
    lv_obj_set_style_text_color(label_prefix, lv_color_hex(0xFFFFFF), 0);
    // 修复字体问题：使用存在的字体 lv_font_montserrat_16
    lv_obj_set_style_text_font(label_prefix, &lv_font_montserrat_16, 0);

    // 输入框
    g_ta_admin_count = lv_textarea_create(input_cont);
    lv_obj_set_size(g_ta_admin_count, 120, 50);
    lv_textarea_set_placeholder_text(g_ta_admin_count, "3");
    lv_textarea_set_max_length(g_ta_admin_count, 3);
    lv_textarea_set_one_line(g_ta_admin_count, true);
    lv_textarea_set_accepted_chars(g_ta_admin_count, "0123456789");
    lv_obj_set_style_border_width(g_ta_admin_count, 1, 0);
    lv_obj_set_style_border_color(g_ta_admin_count, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_ta_admin_count, 5, 0);
    lv_obj_set_style_pad_all(g_ta_admin_count, 8, 0);
    lv_obj_set_style_bg_color(g_ta_admin_count, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(g_ta_admin_count, lv_color_hex(0x000000), 0);
    // 修复字体问题：使用存在的字体 lv_font_montserrat_16
    lv_obj_set_style_text_font(g_ta_admin_count, &lv_font_montserrat_16, 0);
    
    // 设置当前值
    char count_buf[8];
    sprintf(count_buf, "%d", g_admin_count);
    lv_textarea_set_text(g_ta_admin_count, count_buf);

    // 添加键盘完成事件
    lv_obj_add_event_cb(g_ta_admin_count, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_READY) {
            admin_count_confirm_cb(e);
        }
    }, LV_EVENT_READY, NULL);

    // 当前设置显示
    lv_obj_t *current_label = lv_label_create(content);
    char current_buf[64];
    snprintf(current_buf, sizeof(current_buf), "当前设置: %d 个管理员", g_admin_count);
    lv_label_set_text(current_label, current_buf);
    lv_obj_set_style_text_color(current_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_14, 0);

    // 范围提示 - 修复字体问题
    lv_obj_t *range_label = lv_label_create(content);
    lv_label_set_text(range_label, "范围: 1 - 100");
    lv_obj_set_style_text_color(range_label, lv_color_hex(0xAAAAAA), 0);
    // 使用存在的字体 lv_font_montserrat_14 代替 12
    lv_obj_set_style_text_font(range_label, &lv_font_montserrat_14, 0);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 70);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", admin_count_confirm_cb, NULL);
 
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);
   
    lv_obj_add_event_cb(g_btn_cancel, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_cancel = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_ta_admin_count);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_confirm);

    // 加载屏幕
    lv_screen_load(scr_basic_admin_count);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_admin_count);
}

// =================基础设置-记录警告数设置（三级）================
// 警告记录数选项
static const int warn_count_options[] = {10, 50, 100, 200, 500, 1000, 2000, 5000, 10000};
static const char* warn_count_names[] = {
    "10条",
    "50条", 
    "100条",
    "200条",
    "500条",
    "1000条",
    "2000条",
    "5000条",
    "10000条"
};
static const int WARN_COUNT_OPTIONS_NUM = sizeof(warn_count_options) / sizeof(warn_count_options[0]);

// 保存警告记录数设置到系统
static void save_warn_count_to_system(int index) {
    g_warn_count_index = index;
    int count = warn_count_options[index];
    printf("警告记录数已设置为: %s (%d条)\n", warn_count_names[index], count);
    // TODO: 实际保存到系统配置
}

// 下拉框键盘导航回调
static void warn_count_dropdown_keypad_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
    lv_group_t *group = UiManager::getInstance()->getKeypadGroup();
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_UP) {
            if (!lv_dropdown_is_open(dropdown)) {
                lv_group_focus_prev(group);
            }
        } else if (key == LV_KEY_DOWN) {
            if (!lv_dropdown_is_open(dropdown)) {
                lv_group_focus_next(group);
            }
        } else if (key == LV_KEY_ENTER) {
            if (lv_dropdown_is_open(dropdown)) {
                lv_dropdown_close(dropdown);
            } else {
                lv_dropdown_open(dropdown);
            }
        } else if (key == LV_KEY_ESC) {
            if (lv_dropdown_is_open(dropdown)) {
                lv_dropdown_close(dropdown);
            } else {
                load_sys_settings_basic_screen();
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    }
}

// 下拉框值改变回调
static void warn_count_dropdown_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
        int selected = lv_dropdown_get_selected(dropdown);
        
        // 更新预览标签
        if (g_label_warn_preview) {
            char buf[64];
            snprintf(buf, sizeof(buf), "系统最多保存 %s 警告记录", warn_count_names[selected]);
            lv_label_set_text(g_label_warn_preview, buf);
        }
    }
}

// 确认按钮回调
static void warn_count_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        load_sys_settings_basic_screen();
        lv_indev_wait_release(lv_indev_get_act());
        return;
    }
    
    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        int selected = lv_dropdown_get_selected(g_dropdown_warn_count);
        save_warn_count_to_system(selected);
        
        show_popup_msg("成功", "警告记录数已保存", nullptr, "确定");
        load_sys_settings_basic_screen();
    }
}

void load_sys_basic_warn_count_screen() {
    if (scr_basic_warn_count) {
        lv_obj_delete(scr_basic_warn_count);
        scr_basic_warn_count = nullptr;
    }

    BaseScreenParts parts = create_base_screen("记录警告数");
    scr_basic_warn_count = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_BASIC_WARN_COUNT, &scr_basic_warn_count);

    // 屏幕删除回调
    lv_obj_add_event_cb(scr_basic_warn_count, [](lv_event_t *e) {
        scr_basic_warn_count = nullptr;
        g_dropdown_warn_count = nullptr;
        g_label_warn_preview = nullptr;
        g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;

    // 设置内容区域为列布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 15, 0);

    // 提示信息
    lv_obj_t *hint_label = lv_label_create(content);
    lv_label_set_text(hint_label, "设置系统保存的警告记录最大数量");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(hint_label, LV_PCT(100));

    // 当前设置显示
    lv_obj_t *current_label = lv_label_create(content);
    char current_buf[64];
    snprintf(current_buf, sizeof(current_buf), "当前设置: %s", warn_count_names[g_warn_count_index]);
    lv_label_set_text(current_label, current_buf);
    lv_obj_set_style_text_color(current_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(current_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(current_label, LV_PCT(100));

    // ===== 创建警告记录数下拉框 =====
    // 准备下拉框选项数据
    std::vector<std::pair<int, std::string>> warn_count_options_list;
    for (int i = 0; i < WARN_COUNT_OPTIONS_NUM; i++) {
        warn_count_options_list.push_back({i, warn_count_names[i]});
    }
    
    // 使用下拉框函数创建记录数选择
    g_dropdown_warn_count = create_form_dropdown(
        content,                        // 父容器
        "数量",                         // 标签文本
        warn_count_options_list,        // 选项数据
        g_warn_count_index              // 默认选中索引
    );
    
    // 为下拉框添加键盘导航回调
    lv_obj_add_event_cb(g_dropdown_warn_count, warn_count_dropdown_keypad_cb, LV_EVENT_KEY, NULL);
    
    // 为下拉框添加值改变事件
    lv_obj_add_event_cb(g_dropdown_warn_count, warn_count_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 预览标签
    g_label_warn_preview = lv_label_create(content);
    char preview_buf[64];
    snprintf(preview_buf, sizeof(preview_buf), "系统最多保存 %s 警告记录", 
             warn_count_names[g_warn_count_index]);
    lv_label_set_text(g_label_warn_preview, preview_buf);
    lv_obj_set_style_text_color(g_label_warn_preview, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(g_label_warn_preview, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(g_label_warn_preview, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_label_warn_preview, LV_PCT(100));

    // 说明信息
    lv_obj_t *fifo_label = lv_label_create(content);
    lv_label_set_text(fifo_label, "超过数量将自动覆盖最早的记录");
    lv_obj_set_style_text_color(fifo_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(fifo_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(fifo_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(fifo_label, LV_PCT(100));

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 创建确认按钮
    g_btn_confirm = create_form_btn(btn_cont, "确认", warn_count_confirm_cb, NULL);
    lv_obj_add_flag(g_btn_confirm, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 为按钮添加键盘导航回调
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_group_t *group = UiManager::getInstance()->getKeypadGroup();
        
        if (code == LV_EVENT_KEY) {
            uint32_t key = lv_event_get_key(e);
            
            if (key == LV_KEY_UP) {
                lv_group_focus_prev(group);
            } else if (key == LV_KEY_DOWN) {
                lv_group_focus_next(group);
            } else if (key == LV_KEY_ESC) {
                load_sys_settings_basic_screen();
                lv_indev_wait_release(lv_indev_get_act());
            }
        }
    }, LV_EVENT_KEY, NULL);

    // ===== 加入键盘组 =====
    UiManager::getInstance()->addObjToGroup(g_dropdown_warn_count);
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置编辑模式
    lv_group_set_editing(UiManager::getInstance()->getKeypadGroup(), false);
    
    // 为下拉框添加标志，使其可聚焦
    lv_obj_add_flag(g_dropdown_warn_count, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // 设置初始焦点到下拉框
    lv_group_focus_obj(g_dropdown_warn_count);

    // 加载屏幕
    lv_screen_load(scr_basic_warn_count);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic_warn_count);
}

// =================高级设置界面（二级）================
static void sys_advanced_event_cb(lv_event_t *e){
    const char* tag = (const char*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 
    
    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY){

        if(key == LV_KEY_ESC){
          load_sys_settings_menu_screen();
          lv_indev_wait_release(lv_indev_get_act());
          return;
        }

        else if(key == LV_KEY_DOWN || key == LV_KEY_RIGHT){
          lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
          return;
        }

        else if(key == LV_KEY_UP || key == LV_KEY_LEFT){
          lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
          return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)){
        lv_indev_wait_release(lv_indev_get_act());
        
        if(strcmp(tag, "CLEAN_RECORDS") == 0) load_sys_advanced_clean_records_sreen();
        else if(strcmp(tag, "CLEAN_EMPLOYEE") == 0) load_sys_advanced_clean_employee_sreen();
        else if(strcmp(tag, "CLEAN_DATA") == 0) load_sys_advanced_clean_data_sreen();
        else if(strcmp(tag, "FACTORY_RESET") == 0) load_sys_advanced_factory_reset_sreen();
        else if(strcmp(tag, "SYSTEM_UPDATE") == 0) load_sys_advanced_system_update_sreen();
    }

}

void load_sys_settings_advanced_screen(){
    if(scr_advanced){
        lv_obj_delete(scr_advanced);
        scr_advanced = nullptr;
    }

    BaseScreenParts parts = create_base_screen("高级设置");
    scr_advanced = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADVANCED,&scr_advanced);

    lv_obj_add_event_cb(scr_advanced, [](lv_event_t *e){
        scr_advanced = nullptr;
    },LV_EVENT_DELETE,NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* list = create_list_container(parts.content);

    create_sys_list_btn(list, "1. ", "", "清除所有记录", sys_advanced_event_cb, "CLEAN_RECORDS");
    create_sys_list_btn(list, "2. ", "", "清除所有员工", sys_advanced_event_cb, "CLEAN_EMPLOYEE");
    create_sys_list_btn(list, "3. ", "", "清除所有数据", sys_advanced_event_cb, "CLEAN_DATA");
    create_sys_list_btn(list, "4. ", "", "恢复出厂设置", sys_advanced_event_cb, "FACTORY_RESET");
    create_sys_list_btn(list, "5. ", "", "系统升级", sys_advanced_event_cb, "SYSTEM_UPDATE");

    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);
    }

    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_advanced);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced);
}

// ================= 清空所有记录界面 ================
static void clear_records_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_sys_settings_advanced_screen();
            lv_indev_wait_release(lv_indev_get_act());
            return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        lv_obj_t * target = (lv_obj_t*)lv_event_get_target(e);
        const char * tag = (const char*)lv_obj_get_user_data(target);
        
        if(strcmp(tag, "CONFIRM") == 0) {
            //通过UiController调用业务层接口
            UiController::getInstance()->clearAllRecords();
            show_popup_msg("成功", "所有考勤记录已清除", nullptr, "确定");
            load_sys_settings_advanced_screen();
 
        } 
        
        else if(strcmp(tag, "CANCEL") == 0) {
            load_sys_settings_advanced_screen();
        }
    }
}

void load_sys_advanced_clean_records_sreen() {
    if(scr_advanced_clear_records) {
        lv_obj_delete(scr_advanced_clear_records);
        scr_advanced_clear_records = nullptr;
    }

    BaseScreenParts parts = create_base_screen("清除所有记录");
    scr_advanced_clear_records = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADV_CLEAR_RECORDS, &scr_advanced_clear_records);

    lv_obj_add_event_cb(scr_advanced_clear_records, [](lv_event_t *e) {
        scr_advanced_clear_records = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;
    
    // 设置内容区域布局
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 警告文字
    lv_obj_t *warning_label = lv_label_create(content);
    lv_label_set_text(warning_label, "警告");
    lv_obj_set_style_text_color(warning_label, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_top(warning_label, 10, 0);

    // 提示信息
    lv_obj_t *info_label = lv_label_create(content);
    lv_label_set_text(info_label, "此操作将清除所有考勤记录\n且无法恢复！");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, LV_PCT(80));
    lv_obj_set_style_pad_top(info_label, 20, 0);

    // 记录数量统计（可选，需要从数据库查询）
    lv_obj_t *count_label = lv_label_create(content);
    lv_label_set_text(count_label, "当前记录数: 未知");
    lv_obj_set_style_text_color(count_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_pad_top(count_label, 10, 0);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_style_pad_top(btn_cont, 30, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", clear_records_confirm_cb, NULL);

    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_confirm);

    lv_screen_load(scr_advanced_clear_records);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced_clear_records);
}

// ================= 清空所有员工界面 ================
static void clear_employees_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_sys_settings_advanced_screen();
            lv_indev_wait_release(lv_indev_get_act());
            return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        const char* tag = (const char*)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
        
        if(strcmp(tag, "CONFIRM") == 0) {
            //通过UiController调用业务层接口
            UiController::getInstance()->clearAllEmployees();
            show_popup_msg("成功", "所有员工数据已清除", nullptr, "确定");
            load_sys_settings_advanced_screen();
            }

        else if(strcmp(tag, "CANCEL") == 0) {
            load_sys_settings_advanced_screen();
        }
    }
}

void load_sys_advanced_clean_employee_sreen() {
    if(scr_advanced_clear_employees) {
        lv_obj_delete(scr_advanced_clear_employees);
        scr_advanced_clear_employees = nullptr;
    }

    BaseScreenParts parts = create_base_screen("清除所有员工");
    scr_advanced_clear_employees = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADV_CLEAR_EMPLOYEES, &scr_advanced_clear_employees);

    lv_obj_add_event_cb(scr_advanced_clear_employees, [](lv_event_t *e) {
        scr_advanced_clear_employees = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;
    
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 警告文字
    lv_obj_t *warning_label = lv_label_create(content);
    lv_label_set_text(warning_label, "危险操作");
    lv_obj_set_style_text_color(warning_label, lv_color_hex(0xF44336), 0);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_top(warning_label, 10, 0);

    // 提示信息
    lv_obj_t *info_label = lv_label_create(content);
    lv_label_set_text(info_label, "此操作将删除所有员工信息\n包括人脸数据和考勤记录\n且无法恢复！");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, LV_PCT(80));
    lv_obj_set_style_pad_top(info_label, 20, 0);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_style_pad_top(btn_cont, 30, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", clear_employees_confirm_cb, NULL);
  
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_cancel);

    lv_screen_load(scr_advanced_clear_employees);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced_clear_employees);
}

// ================= 清空所有数据界面 ================
static void clear_all_data_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_sys_settings_advanced_screen();
            lv_indev_wait_release(lv_indev_get_act());
            return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        lv_obj_t * target = (lv_obj_t*)lv_event_get_target(e); 
        const char * tag = (const char*)lv_obj_get_user_data(target);
        
        if(strcmp(tag, "CONFIRM") == 0) {
            //通过UiController调用业务层接口
            UiController::getInstance()->clearAllData();
            show_popup_msg("成功", "所有数据已清除", nullptr, "确定");
            load_sys_settings_advanced_screen();
            
        } 
        
        else if(strcmp(tag, "CANCEL") == 0) {
            load_sys_settings_advanced_screen();
        }
    }
}

void load_sys_advanced_clean_data_sreen() {
        if(scr_advanced_clear_all_data) {
        lv_obj_delete(scr_advanced_clear_all_data);
        scr_advanced_clear_all_data = nullptr;
    }

    BaseScreenParts parts = create_base_screen("清除所有数据");
    scr_advanced_clear_all_data = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADV_CLEAR_ALL_DATA, &scr_advanced_clear_all_data);

    lv_obj_add_event_cb(scr_advanced_clear_all_data, [](lv_event_t *e) {
        scr_advanced_clear_all_data = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;
    
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 警告文字
    lv_obj_t *warning_label = lv_label_create(content);
    lv_label_set_text(warning_label, "危险操作");
    lv_obj_set_style_text_color(warning_label, lv_color_hex(0xF44336), 0);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_top(warning_label, 10, 0);

    // 提示信息
    lv_obj_t *info_label = lv_label_create(content);
    lv_label_set_text(info_label, "此操作将删除所有数据记录\n且无法恢复！");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, LV_PCT(80));
    lv_obj_set_style_pad_top(info_label, 20, 0);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_style_pad_top(btn_cont, 30, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", clear_all_data_confirm_cb, NULL);
        
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_cancel);

    lv_screen_load(scr_advanced_clear_all_data);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced_clear_all_data);
}

// ================= 恢复出厂设置界面 ================
static void factory_reset_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_sys_settings_advanced_screen();
            lv_indev_wait_release(lv_indev_get_act());
            return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        lv_obj_t * target = (lv_obj_t*)lv_event_get_target(e);
        const char * tag = (const char*)lv_obj_get_user_data(target);
        
        if(strcmp(tag, "CONFIRM") == 0) {
            //通过UiController调用业务层接口
            UiController::getInstance()->factoryReset();
            show_popup_msg("成功", "已恢复出厂设置\n系统即将重启", nullptr, "确定");
            load_sys_settings_advanced_screen(); 
           
        } 
        
        else if(strcmp(tag, "CANCEL") == 0) {
            load_sys_settings_advanced_screen();
        }
    }
}

void load_sys_advanced_factory_reset_sreen() {
    if(scr_advanced_factory_reset) {
        lv_obj_delete(scr_advanced_factory_reset);
        scr_advanced_factory_reset = nullptr;
    }

    BaseScreenParts parts = create_base_screen("恢复出厂设置");
    scr_advanced_factory_reset = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADV_FACTORY_RESET, &scr_advanced_factory_reset);

    lv_obj_add_event_cb(scr_advanced_factory_reset, [](lv_event_t *e) {
        scr_advanced_factory_reset = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;
    
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 操作名称
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "恢复出厂设置");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_top(title_label, 10, 0);

    // 详细信息
    lv_obj_t *info_label = lv_label_create(content);
    lv_label_set_text(info_label, " 此操作将：\n"
                                "1. 清除所有员工数据\n"
                                "2. 清除所有考勤记录\n"
                                "3. 重置系统设置\n"
                                "4. 重启设备");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, LV_PCT(80));
    lv_obj_set_style_pad_top(info_label, 20, 0);

    // 最终确认提示
    lv_obj_t *final_warning = lv_label_create(content);
    lv_label_set_text(final_warning, "此操作不可逆，请确认！");
    lv_obj_set_style_text_color(final_warning, lv_color_hex(0xF44336), 0);
    lv_obj_set_style_pad_top(final_warning, 20, 0);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_style_pad_top(btn_cont, 20, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 确认按钮
    g_btn_confirm = create_form_btn (btn_cont, "确认", factory_reset_confirm_cb, NULL);
    
    lv_obj_add_event_cb(g_btn_confirm, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) g_btn_confirm = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(g_btn_confirm);

    // 设置初始焦点
    lv_group_focus_obj(g_btn_cancel);

    lv_screen_load(scr_advanced_factory_reset);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced_factory_reset);
}

// ================= 系统升级界面 ================
static void upgrade_check_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_sys_settings_advanced_screen();
            lv_indev_wait_release(lv_indev_get_act());
            return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());
        
        lv_obj_t * target = (lv_obj_t*)lv_event_get_target(e);
        const char * tag = (const char*)lv_obj_get_user_data(target);
        
        if(strcmp(tag, "CHECK") == 0) {
            // 检查更新
            show_popup_msg("提示", "正在检查更新...", nullptr, "确定");
            load_sys_settings_advanced_screen();
        } 
        
        else if(strcmp(tag, "UPGRADE") == 0) {
            // 开始升级
            show_popup_msg("提示", "开始系统升级...", nullptr, "确定");
            load_sys_settings_advanced_screen();
        } 
        
        else if(strcmp(tag, "BACK") == 0) {
            load_sys_settings_advanced_screen();
        }
    }
}

void load_sys_advanced_system_update_sreen() {
    if(scr_advanced_upgrade) {
        lv_obj_delete(scr_advanced_upgrade);
        scr_advanced_upgrade = nullptr;
    }

    BaseScreenParts parts = create_base_screen("系统升级");
    scr_advanced_upgrade = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADV_UPGRADE, &scr_advanced_upgrade);

    lv_obj_add_event_cb(scr_advanced_upgrade, [](lv_event_t *e) {
        scr_advanced_upgrade = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t *content = parts.content;
    
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 当前版本信息
    lv_obj_t *current_version = lv_label_create(content);
    lv_label_set_text(current_version, "当前版本: V2.0.0");
    lv_obj_set_style_text_font(current_version, &lv_font_montserrat_16, 0);

    // 最新版本信息
    lv_obj_t *latest_version = lv_label_create(content);
    lv_label_set_text(latest_version, "最新版本: 未知");
    lv_obj_set_style_text_color(latest_version, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_pad_top(latest_version, 10, 0);

    // 升级状态
    lv_obj_t *status_label = lv_label_create(content);
    lv_label_set_text(status_label, "未检测到新版本");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_pad_top(status_label, 20, 0);

    // 进度条（升级时显示）
    lv_obj_t *progress_bar = lv_bar_create(content);
    lv_obj_set_size(progress_bar, 200, 20);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_pad_top(progress_bar, 20, 0);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);

    // 进度文本
    lv_obj_t *progress_label = lv_label_create(content);
    lv_label_set_text(progress_label, "0%");
    lv_obj_set_style_pad_top(progress_label, 5, 0);
    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);

    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(content);
    lv_obj_set_size(btn_cont, LV_PCT(90), 60);
    lv_obj_set_style_pad_top(btn_cont, 30, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, LV_PART_MAIN); 
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 检查更新按钮
    lv_obj_t *btn_check = create_form_btn(btn_cont, "检查", upgrade_check_cb, NULL);
    lv_obj_set_size(btn_check, 60, 40);
    lv_obj_add_event_cb(btn_check, upgrade_check_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_check, upgrade_check_cb, LV_EVENT_KEY, NULL);

    // 开始升级按钮
    lv_obj_t *btn_upgrade = create_form_btn(btn_cont, "升级", upgrade_check_cb, NULL);
    lv_obj_set_size(btn_upgrade, 60, 40);
    lv_obj_add_event_cb(btn_upgrade, upgrade_check_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_upgrade, upgrade_check_cb, LV_EVENT_KEY, NULL);

    // 加入键盘组
    UiManager::getInstance()->addObjToGroup(btn_check);
    UiManager::getInstance()->addObjToGroup(btn_upgrade);

    // 设置初始焦点
    lv_group_focus_obj(btn_check);

    lv_screen_load(scr_advanced_upgrade);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced_upgrade);
}

// =================自检设置界面（二级）================
static void sys_selfcheck_event_cb(lv_event_t *e){
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 
    
    uint32_t key = 0;

    if(code == LV_EVENT_KEY){
        key = lv_event_get_key(e);
    }

    if(code == LV_EVENT_KEY){

        if(key == LV_KEY_ESC){
          load_sys_settings_menu_screen();
          lv_indev_wait_release(lv_indev_get_act());
          return;
        }

        else if(key == LV_KEY_DOWN || key == LV_KEY_RIGHT){
          lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
          return;
        }

        else if(key == LV_KEY_UP || key == LV_KEY_LEFT){
          lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
          return;
        }
    }

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)){
        lv_indev_wait_release(lv_indev_get_act());
        load_sys_settings_selfcheck_screen();
    }
}

void load_sys_settings_selfcheck_screen(){
    if(scr_self_check){
        lv_obj_delete(scr_self_check);
        scr_self_check = nullptr;
    }

    BaseScreenParts parts = create_base_screen("自检功能");
    scr_self_check = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_SELFCHECK,&scr_self_check);

    lv_obj_add_event_cb(scr_self_check, [](lv_event_t *e){
        scr_self_check = nullptr;
    },LV_EVENT_DELETE,NULL);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* list = create_list_container(parts.content);

    create_sys_list_btn(list, "1. ", "", "指纹采集器", sys_selfcheck_event_cb, "FINGERPRINT");
    create_sys_list_btn(list, "2. ", "", "红外摄像头", sys_selfcheck_event_cb, "IR_CAMERA");
    create_sys_list_btn(list, "3. ", "", "彩色摄像头", sys_selfcheck_event_cb, "COLOR_CAMERA");

    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);
    }

    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_self_check);
    UiManager::getInstance()->destroyAllScreensExcept(scr_self_check);
}

} // namespace system
} // namespace ui
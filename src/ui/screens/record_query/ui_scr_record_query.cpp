#include "ui_scr_record_query.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"
#include <cstdio>
#include <string>
#include <vector>
#include <ctime>

namespace ui {
namespace record_query {

static lv_obj_t *scr_query = nullptr;
static lv_obj_t *scr_result = nullptr;

// 输入页控件
static lv_obj_t *ta_query_id = nullptr;
static lv_obj_t *btn_query_back = nullptr;

// ================= [Input Screen] =================

// [Critical Logic] 手动处理焦点跳转，防止被 Textarea 困住
static void query_screen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        // 1. 如果焦点在 输入框
        if (target == ta_query_id) {
            if (key == LV_KEY_DOWN) {
                // 强制跳到返回按钮
                if(btn_query_back) lv_group_focus_obj(btn_query_back);
            } 
            else if (key == LV_KEY_ENTER) {
                // 执行查询
                const char* txt = lv_textarea_get_text(ta_query_id);
                if (strlen(txt) > 0) {
                    load_record_result_screen(atoi(txt));
                }
            }
            else if (key == LV_KEY_ESC) {
                ui::menu::load_screen();
            }
        }
        // 2. 如果焦点在 返回按钮
        else if (target == btn_query_back) {
            if (key == LV_KEY_UP) {
                // 强制跳回输入框
                if(ta_query_id) lv_group_focus_obj(ta_query_id);
            }
            else if (key == LV_KEY_ENTER || key == LV_KEY_ESC) {
                ui::menu::load_screen();
            }
        }
    }
}

// 主屏幕实现
void load_record_query_menu_screen() {
    if (scr_query) lv_obj_delete(scr_query);
    scr_query = lv_obj_create(nullptr);
    lv_obj_add_style(scr_query, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::RECORD_QUERY, &scr_query);

    lv_obj_t *title = lv_label_create(scr_query);
    lv_label_set_text(title, "记录查询 / Query");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 输入框
    ta_query_id = lv_textarea_create(scr_query);
    lv_textarea_set_one_line(ta_query_id, true);
    lv_textarea_set_placeholder_text(ta_query_id, "输入工号 ID");
    lv_textarea_set_accepted_chars(ta_query_id, "0123456789");
    lv_textarea_set_max_length(ta_query_id, 8);
    lv_obj_set_width(ta_query_id, 200);
    lv_obj_align(ta_query_id, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_style(ta_query_id, &style_focus_red, LV_STATE_FOCUSED);
    // 绑定事件用于处理 Enter 和手动导航
    lv_obj_add_event_cb(ta_query_id, query_screen_event_cb, LV_EVENT_KEY, nullptr);

    // 提示文本
    lv_obj_t *lbl_tip = lv_label_create(scr_query);
    lv_label_set_text(lbl_tip, "Press Enter to Search");
    lv_obj_set_style_text_color(lbl_tip, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lbl_tip, LV_ALIGN_CENTER, 0, 10);

    // 返回按钮
    btn_query_back = lv_button_create(scr_query);
    lv_obj_set_size(btn_query_back, 100, 40);
    lv_obj_align(btn_query_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_style(btn_query_back, &style_focus_red, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn_query_back, query_screen_event_cb, LV_EVENT_KEY, nullptr);
    
    lv_obj_t *lbl_back = lv_label_create(btn_query_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);

    // Group 设置
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(ta_query_id);
    UiManager::getInstance()->addObjToGroup(btn_query_back);
    lv_group_focus_obj(ta_query_id); // 默认聚焦输入框

    lv_screen_load(scr_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_query);
}

// ================= [Result Screen] =================

// 主屏幕实现
void load_record_result_screen(int user_id) {
    if (scr_result) lv_obj_delete(scr_result);
    scr_result = lv_obj_create(nullptr);
    lv_obj_add_style(scr_result, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::RECORD_RESULT, &scr_result);

    lv_obj_t *title = lv_label_create(scr_result);
    lv_label_set_text_fmt(title, "User: %d Records", user_id);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    // 结果容器 (Flex Column)
    lv_obj_t *cont = lv_obj_create(scr_result);
    lv_obj_set_size(cont, LV_PCT(95), LV_PCT(75));
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    // 获取数据
    std::vector<AttendanceRecord> records = UiController::getInstance()->getRecords(user_id, 0, 2147483647);

    UiManager::getInstance()->resetKeypadGroup();

    if (records.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No Records Found");
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_obj_center(lbl);
    } else {
        for (const auto& r : records) {
            lv_obj_t *item = lv_obj_create(cont);
            lv_obj_set_size(item, LV_PCT(100), 30);
            lv_obj_remove_style_all(item); // 纯净容器
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            
            // 时间
            lv_obj_t *l1 = lv_label_create(item);
           
            char time_buf[64];
            time_t t_val = (time_t)r.timestamp; // 1. 创建本地变量并强转类型
            struct tm *tm_info = localtime(&t_val); // 2. 传递本地变量的地址
            if (tm_info) {
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", tm_info);
            } else {
                strcpy(time_buf, "Unknown Time");
            }
            
            lv_label_set_text(l1, time_buf); // 格式化时间显示

            lv_obj_set_style_text_color(l1, lv_color_white(), 0);
            
            // 状态 (迟到/早退/正常)
            lv_obj_t *l2 = lv_label_create(item);
            // 简单逻辑判断状态显示颜色
            const char* status_txt = "Normal";
            lv_color_t color = lv_palette_main(LV_PALETTE_GREEN);
            if(r.status == 1) { status_txt="Late"; color=lv_palette_main(LV_PALETTE_ORANGE); }
            else if(r.status == 2) { status_txt="Early"; color=lv_palette_main(LV_PALETTE_ORANGE); }
            
            lv_label_set_text(l2, status_txt);
            lv_obj_set_style_text_color(l2, color, 0);

            // 让列表项可聚焦以便滚动
            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_style(item, &style_focus_red, LV_STATE_FOCUSED);
            UiManager::getInstance()->addObjToGroup(item);
        }
    }

    // 底部返回按钮
    lv_obj_t *btn_back = lv_button_create(scr_result);
    lv_obj_set_size(btn_back, LV_PCT(100), 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(lv_label_create(btn_back), "Back to Query");
    lv_obj_add_style(btn_back, &style_focus_red, LV_STATE_FOCUSED);
    
    // 事件：ESC或点击返回
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ENTER) load_record_query_menu_screen();
    }, LV_EVENT_KEY, nullptr);
    
    lv_obj_add_event_cb(scr_result, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_record_query_menu_screen();
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->addObjToGroup(btn_back);
    // 默认聚焦第一条记录或返回按钮
    if(!records.empty()) lv_group_focus_obj(lv_obj_get_child(cont, 0));
    else lv_group_focus_obj(btn_back);

    lv_screen_load(scr_result);
    UiManager::getInstance()->destroyAllScreensExcept(scr_result);
}

} // namespace record_query
} // namespace ui
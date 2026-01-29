#include "ui_scr_home.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../menu/ui_scr_menu.h"
#include "../../business/event_bus.h" 
#include <any>                        
#include <string>
#include <cstdio>

// 引用字体
LV_FONT_DECLARE(font_noto_16);

namespace ui {
namespace home {

static lv_obj_t *scr_home = nullptr;
static lv_obj_t *img_camera = nullptr;
static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *lbl_date = nullptr;
static lv_obj_t *lbl_disk_warn = nullptr;
static lv_timer_t *timer_cam = nullptr;     // 刷新定时器

// ================= [EventBus 回调] =================

// 注意：EventBus 回调可能在非 UI 线程触发，建议使用 lv_async_call 转发，
// 但如果 EventBus 保证在主线程 publish，则可直接调用。
// 这里假设 EventBus 在业务线程，我们使用 LVGL 线程安全机制 (lv_async_call) 是最稳妥的。

struct TimeData { std::string t; std::string d; };

// 时间更新回调 (UI 线程)
static void on_time_update_ui_thread(void* data) {
    TimeData* td = (TimeData*)data;
    if (scr_home && lbl_time && lbl_date) {
        lv_label_set_text(lbl_time, td->t.c_str());
        lv_label_set_text(lbl_date, td->d.c_str());
    }
    delete td;
}

// 事件总线时间更新回调
static void on_time_update_bus(const std::any& data) {
    
    try {

        auto time_str = std::any_cast<std::string>(data);
        
        // 转发到 UI 线程
        TimeData* td = new TimeData{time_str, "2024-01-01"}; // 日期暂定
        lv_async_call(on_time_update_ui_thread, td);
    } catch (...) {
        // 忽略类型转换错误
    }
}

// 磁盘状态更新回调 (UI 线程)
static void on_disk_full_ui_thread(void* data) {
    bool is_full = (bool)(intptr_t)data;
    if (scr_home && lbl_disk_warn) {
        if (is_full) lv_obj_remove_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN);
    }
}

// 事件总线磁盘状态回调
static void on_disk_status_bus(const std::any& data) {
    // 假设 data 是 bool
    try {
        bool is_full = std::any_cast<bool>(data);
        lv_async_call(on_disk_full_ui_thread, (void*)(intptr_t)is_full);
    } catch(...) {}
}

// ================= [刷新逻辑] =================

// 摄像头图像刷新定时器回调
static void camera_refresh_timer_cb(lv_timer_t *t) {
    if (img_camera && lv_obj_is_valid(img_camera)) {
        lv_obj_invalidate(img_camera);
    }
}

// 主屏幕按键事件回调
static void home_event_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ENTER || lv_event_get_key(e) == LV_KEY_ESC) { 
        ui::menu::load_screen();
    }
}

// ================= [加载逻辑] =================

// 主屏幕实现
void load_screen() {
    if (scr_home) lv_obj_delete(scr_home);
    scr_home = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_home, lv_color_black(), 0);
    
    UiManager::getInstance()->registerScreen(ScreenType::MAIN, &scr_home);

    // 1. 摄像头预览
    static lv_image_dsc_t img_dsc;
    img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
    img_dsc.header.w = 320;
    img_dsc.header.h = 240;
    img_dsc.header.stride = 320 * 3;
    img_dsc.data_size = 320 * 240 * 3;
    img_dsc.data = UiManager::getInstance()->getCameraDisplayBuffer();

    img_camera = lv_image_create(scr_home);
    lv_image_set_src(img_camera, &img_dsc);
    lv_obj_align(img_camera, LV_ALIGN_CENTER, 0, 10);

    // 2. 顶部栏
    lv_obj_t *top_bar = lv_obj_create(scr_home);
    lv_obj_set_size(top_bar, LV_PCT(100), 40);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);

    lbl_time = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_align(lbl_time, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_label_set_text(lbl_time, "00:00"); 

    lbl_date = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_date, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lbl_date, LV_ALIGN_LEFT_MID, 10, 0);
    lv_label_set_text(lbl_date, "2024-01-01");

    // 3. 磁盘警告
    lbl_disk_warn = lv_label_create(scr_home);
    lv_label_set_text(lbl_disk_warn, "Disk Full!");
    lv_obj_set_style_text_color(lbl_disk_warn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lbl_disk_warn, LV_ALIGN_TOP_MID, 0, 45); 
    lv_obj_add_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN);

    // 4. 底部提示
    lv_obj_t *lbl_hint = lv_label_create(scr_home);
    lv_label_set_text(lbl_hint, "Ready / 请正对摄像头");
    lv_obj_add_style(lbl_hint, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl_hint, lv_color_white(), 0);
    lv_obj_align(lbl_hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 5. 交互
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(scr_home);
    lv_group_focus_obj(scr_home);
    lv_obj_add_event_cb(scr_home, home_event_cb, LV_EVENT_KEY, nullptr);

    // 定时器
    timer_cam = lv_timer_create(camera_refresh_timer_cb, 50, nullptr);
    
    // 订阅逻辑
    EventBus::getInstance().subscribe(EventType::TIME_UPDATE, on_time_update_bus);
    EventBus::getInstance().subscribe(EventType::DISK_FULL, on_disk_status_bus);

    // 6. 清理逻辑 (Screen 销毁时退订或停止定时器)
    lv_obj_add_event_cb(scr_home, [](lv_event_t* e){
        if(timer_cam) {
            lv_timer_del(timer_cam);
            timer_cam = nullptr;
        }
        // 严格来说应该 Unsubscribe，但 EventBus 单例通常存活整个周期
        // 如果 EventBus 支持 unsubscribe，请在此处调用
    }, LV_EVENT_DELETE, nullptr);

    // 启动刷新定时器 (33ms = ~30FPS)
    if (timer_cam) lv_timer_del(timer_cam); // 防止重复创建
    timer_cam = lv_timer_create(camera_refresh_timer_cb, 33, nullptr);

    lv_screen_load(scr_home);
    UiManager::getInstance()->destroyAllScreensExcept(scr_home);
}

// 接口实现 (如果有外部直接调用需求)
void update_time(const std::string& time_str, const std::string& date_str) {
    if (scr_home && lbl_time && lbl_date) {
        lv_label_set_text(lbl_time, time_str.c_str());
        lv_label_set_text(lbl_date, date_str.c_str());
    }
}

// 磁盘状态更新接口
void update_disk_status(bool is_full) {
    if (scr_home && lbl_disk_warn) {
        if (is_full) lv_obj_remove_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace home
} // namespace ui
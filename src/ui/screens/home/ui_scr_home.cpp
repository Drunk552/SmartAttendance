/**
 * @file ui_scr_home.cpp
 * @brief 主页/摄像头预览界面 - 严格复刻 1.0 布局
 */

#include "ui_scr_home.h"
#include <lvgl.h>
#include <cstdio>
#include <string>

// 模块依赖
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../ui_controller.h"
#include "../../../business/event_bus.h"
#include "../menu/ui_scr_menu.h"

// 在命名空间外部声明，确保链接到 main.cpp 中的全局变量
extern "C" {
    extern volatile bool g_program_should_exit;
}

// 宏定义\
#define SCREEN_W 240
#define SCREEN_H 320
#define CAM_W 240
#define CAM_H 180

namespace ui {
namespace home {

static lv_obj_t * screen = nullptr;
static lv_obj_t * img_camera = nullptr;
static lv_obj_t * lbl_time = nullptr;
static lv_obj_t * lbl_disk_warn = nullptr;
static lv_obj_t * lbl_hint = nullptr;

// 摄像头图像描述符 (v9 格式)
static lv_image_dsc_t img_dsc = {
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

/**
 * @brief 页面主事件回调
 */
static void screen_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            // 等待按键释放以防连跳
            lv_indev_wait_release(lv_indev_get_act());
            // 跳转到菜单页逻辑 (假设已实现)
            ui::menu::load_screen(); 
        }
        if (key == LV_KEY_ESC) {
            // 退出程序请求
            g_program_should_exit = true;
        }
    }
}

/**
 * @brief 摄像头刷新定时器逻辑
 */
static void timer_cam_cb(lv_timer_t * t) {
    if (lv_screen_active() == screen && img_camera) {
        // 使用 UiManager 的同步机制
        if (UiManager::getInstance()->trySetFramePending()) {
            // 获取最新帧到共享 Buffer 并使对象失效触发重绘
            UiController::getInstance()->getDisplayFrame(
                UiManager::getInstance()->getCameraDisplayBuffer(), CAM_W, CAM_H);
            lv_obj_invalidate(img_camera);
            UiManager::getInstance()->clearFramePending();
        }
    }
}

void create_screen(void) {
    if (screen) return;

    // 1. 创建基础屏幕
    screen = lv_obj_create(nullptr);
    lv_obj_add_style(screen, &style_base, 0); 
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(screen, screen_event_cb, LV_EVENT_ALL, nullptr);

    // 2. Top Bar (30px, 深灰背景) 
    lv_obj_t * top = lv_obj_create(screen);
    lv_obj_set_size(top, SCREEN_W, 30);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x333333), 0); 
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);

    // 时间标签 (居中)
    lbl_time = lv_label_create(top);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(lbl_time, THEME_COLOR_TEXT_MAIN, 0);

    // 磁盘警告 (右上角)
    lbl_disk_warn = lv_label_create(top);
    lv_label_set_text(lbl_disk_warn, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(lbl_disk_warn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lbl_disk_warn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

    // 3. Camera (240x180, Y=40) 
    img_dsc.data = UiManager::getInstance()->getCameraDisplayBuffer();
    img_camera = lv_image_create(screen);
    lv_image_set_src(img_camera, &img_dsc);
    lv_obj_set_size(img_camera, CAM_W, CAM_H);
    lv_obj_align(img_camera, LV_ALIGN_TOP_MID, 0, 40);

    // 4. Bottom Bar (110px, Panel Color) 
    lv_obj_t * bottom = lv_obj_create(screen);
    lv_obj_set_size(bottom, SCREEN_W, 110);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom, THEME_COLOR_PANEL, 0); // 确保 THEME_COLOR_PANEL 已定义
    lv_obj_set_style_border_width(bottom, 0, 0);
    lv_obj_set_style_radius(bottom, 0, 0);
    
    // 操作提示 (黄色文本)
    lv_obj_t *lbl_hint = lv_label_create(bottom);
    lv_label_set_text(lbl_hint, "Enter: Menu\nESC: Exit"); 
    lv_obj_set_style_text_color(lbl_hint, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_set_style_text_align(lbl_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_hint, LV_ALIGN_CENTER, 0, 0);

    // 5. 注册到管理器
    UiManager::getInstance()->registerScreen(ScreenType::MAIN, &screen);
}

void load_screen(void) {
    if (!screen) create_screen();

    // 绑定输入设备组
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(screen);
    lv_group_focus_obj(screen);

    // 页面订阅逻辑 (保持 2.0 异步更新)
    auto& bus = EventBus::getInstance();
    bus.subscribe(EventType::TIME_UPDATE, [](void* data) {
        std::string* t = static_cast<std::string*>(data);
        lv_async_call([](void* d){
            if(lbl_time) lv_label_set_text(lbl_time, (const char*)d);
            delete (std::string*)d;
        }, new std::string(*t));
    });

    bus.subscribe(EventType::DISK_FULL, [](void*){
        lv_async_call([](void*){ if(lbl_disk_warn) lv_obj_remove_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN); }, nullptr);
    });

    // 启动定时器刷新摄像头
    lv_timer_create(timer_cam_cb, 33, nullptr);

    lv_screen_load(screen);
    UiManager::getInstance()->destroyAllScreensExcept(screen);
}

} // namespace home
} // namespace ui
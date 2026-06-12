/**
 * @file ui_app.cpp
 * @brief UI 入口 (WSL2/PC 仿真版)
 * @details 使用 SDL2 驱动显示窗口和接收输入，而非读写底层设备节点
 */

#include "ui_app.h"
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>

// 模块接口
#include "managers/ui_manager.h"
#include "ui_controller.h"
#include "common/ui_style.h"
#include "porting/hal_keypad.h"

// 引入主页头文件
#include "screens/home/ui_scr_home.h"

// 屏幕宏定义 (默认 240x320，可按需修改)
#ifndef SCREEN_W
#define SCREEN_W 240
#endif

#ifndef SCREEN_H
#define SCREEN_H 320
#endif

// 全局退出标志 (定义在 main.cpp)
extern "C" {
    extern volatile bool g_program_should_exit;
}

void ui_init(void) {
    printf(">>> [UI] 初始化 (WSL2 SDL仿真版)...\n");

    // 1. 防止 SDL 窗口黑屏休眠
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "0", 1);

    // ============================================================
    // 2. LVGL & HAL 初始化 (使用 SDL 驱动)
    // ============================================================
    lv_init();

    // 创建 SDL 窗口 (模拟显示屏)
    // 这一步需要在 lv_conf.h 中启用 LV_USE_SDL
    lv_display_t * disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
    if (disp) {
        printf("[UI] SDL Window Created (%dx%d).\n", SCREEN_W, SCREEN_H);
    } else {
        printf("[UI] [FATAL] 无法创建 SDL 窗口！请检查环境或 lv_conf.h 配置。\n");
        // 如果这里失败，通常是因为 WSLg 没配置好，或者没装 libsdl2-dev
    }

    // 创建 SDL 输入设备 (鼠标 + 键盘)
    lv_indev_t * mouse = lv_sdl_mouse_create();
    lv_indev_t * kbd   = lv_sdl_keyboard_create();

    // ============================================================
    // 3. 管理器初始化 (创建 Group)
    // ============================================================
    ui_style_init();
    UiManager::getInstance()->init(); // 内部创建 g_keypad_group

    // ============================================================
    // 4. 初始化物理矩阵键盘并绑定到 UI 焦点组
    // ============================================================
    lv_indev_t * keypad_indev = hal_keypad_init();
    if (keypad_indev) {
        lv_group_t * g = UiManager::getInstance()->getKeypadGroup();
        if (g) {
            lv_indev_set_group(keypad_indev, g);
            printf("[UI] 物理矩阵键盘已成功绑定到焦点组！\n");
        }
    }

    // ============================================================
    // 4. 绑定键盘到 UI (解决无法操作菜单的问题)
    // ============================================================
    if (kbd) {
        // [关键] 必须设为 KEYPAD 模式
        lv_indev_set_type(kbd, LV_INDEV_TYPE_KEYPAD);
        
        // 获取全局 Group 并绑定
        lv_group_t * g = UiManager::getInstance()->getKeypadGroup();
        if (g) {
            lv_indev_set_group(kbd, g);
            lv_group_set_wrap(g, true); // 开启循环跳转
            printf("[UI] Keyboard bound to Manager Group.\n");
        }
    } else {
        printf("[UI] [Error] Failed to create SDL Keyboard!\n");
    }

    // ============================================================
    // 5. 启动业务与加载主页
    // ============================================================
    UiController::getInstance()->startBackgroundServices();

    printf("[UI] Loading Home Screen...\n");
    // 直接调用模块加载函数
    ui::home::load_screen();

    UiController::getInstance()->startBackgroundServices();
    printf("[UI] Initialization Completed.\n");
}
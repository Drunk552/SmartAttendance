#include "ui_manager.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

UiManager* UiManager::getInstance() {
    static UiManager instance;
    return &instance;
}

UiManager::UiManager() {
    // 构造时初始化缓冲区（可选，std::array 自动管理内存）
}

// --- 输入设备管理实现 ---
void UiManager::init() {
    // 1. 创建组
    g_keypad_group = lv_group_create();
    lv_group_set_wrap(g_keypad_group, true); // 开启循环模式

    // 2. 初始化输入设备
    // 注意：这里需要依赖 SDL 驱动，假设在 main 或 ui_init 外部已经 setup 好了 SDL
    // 这里我们尝试获取键盘并绑定
    lv_indev_t * ind = lv_indev_get_next(nullptr);
    while(ind) {
        if(lv_indev_get_type(ind) == LV_INDEV_TYPE_KEYPAD || 
           lv_indev_get_type(ind) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(ind, g_keypad_group);
        }
        ind = lv_indev_get_next(ind);
    }
}

// 重置组
void UiManager::resetKeypadGroup() {
    if (g_keypad_group) {
        lv_group_remove_all_objs(g_keypad_group);
    }
}

void UiManager::addObjToGroup(lv_obj_t* obj) {
    if (g_keypad_group && obj) {
        lv_group_add_obj(g_keypad_group, obj);
    }
}

// --- 摄像头缓冲区管理实现 ---
void UiManager::updateCameraFrame(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_frame_mutex);
    
    // 确保不会溢出
    if (size <= cam_buf_display.size()) {
        std::memcpy(cam_buf_display.data(), data, size);
    }
}

// --- 核心内存管理逻辑 ---

// 注册屏幕
void UiManager::registerScreen(ScreenType type, lv_obj_t** screen_ptr_ref) {
    // 检查是否已存在
    for (auto& ms : managed_screens) {
        if (ms.ptr_ref == screen_ptr_ref) return;
    }
    managed_screens.push_back({type, screen_ptr_ref});
    std::printf("[UiManager] Screen Registered. Total: %zu\n", managed_screens.size());
}

// 清理特定屏幕资源
void UiManager::freeScreenResources(lv_obj_t** screen_ptr) {
    if (screen_ptr == nullptr || *screen_ptr == nullptr) return;

    // 1. 销毁 LVGL 对象 (这会递归销毁所有子对象)
    std::printf("[Memory] Freeing Screen %p...\n", *screen_ptr);
    lv_obj_delete(*screen_ptr);
    
    // 2. 将屏幕指针置空，标记为已销毁
    // 这一步至关重要，防止重复释放或野指针访问
    *screen_ptr = nullptr;
    
    std::printf("[Memory] Screen Freed.\n");
}

/**
 * @brief 真正的清理逻辑，由定时器回调执行
 * 此时之前的事件处理已彻底完成，可以安全销毁旧对象
 */
void UiManager::async_screen_cleanup_cb(lv_timer_t * t) {
    UiManager* mgr = (UiManager*)lv_timer_get_user_data(t);
    if (!mgr) return;

    // 1. 获取当前系统真正正在显示的屏幕
    lv_obj_t * act_scr = lv_screen_active(); 
    
    // 2. 获取我们要保留的屏幕 (通常就是 act_scr，但为了保险起见单独传参)
    // 这里的 trick 是我们没法通过 timer data 传两个指针，所以我们稍微改写一下逻辑：
    // 我们遍历 managed_screens，只要不是 act_scr 且不为空，就干掉。
    
    // 注意：原代码逻辑是传入 keep_scr。为了保持一致性，我们这里稍微调整一下设计：
    // 由于是成员函数，mgr 指针是必须的，我们没法把 keep_scr 也塞进 user_data。
    // 但是！我们可以利用 lv_screen_active()。通常 destroy_all_screens_except 都是在 load_screen 之后调用的。
    // 所以 keep_scr 基本等于 act_scr。
    
    for (auto& ms : mgr->managed_screens) {
        lv_obj_t** ptr = ms.ptr_ref;
        // 如果屏幕已创建(非空) 且 不是当前活跃屏幕，则销毁
        if (ptr && *ptr != nullptr && *ptr != act_scr) {
             mgr->freeScreenResources(ptr);
        }
    }
    
    // 任务完成，不需要重复
    // 定时器由 lv_timer_set_repeat_count(t, 1) 自动销毁，不需要手动 del
}

// 启动异步销毁任务
void UiManager::destroyAllScreensExcept(lv_obj_t* keep_scr) {
    // 创建一个 10ms 的单次定时器
    // 这样可以确保当前的按键事件回调完全执行完毕后，再执行销毁
    
    // 注意：我们将 'this' 传给回调，以便访问 managed_screens
    lv_timer_t * t = lv_timer_create(async_screen_cleanup_cb, 10, this);
    lv_timer_set_repeat_count(t, 1); // 只运行一次
}
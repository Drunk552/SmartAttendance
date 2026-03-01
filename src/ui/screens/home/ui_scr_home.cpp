/**
 * @file ui_scr_home.cpp
 * @brief 主页/摄像头预览界面 - 严格复刻 1.0 布局
 */

#include "ui_scr_home.h"
#include <lvgl.h>
#include <cstdio>
#include <string>
#include <ctime>

// 模块依赖
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
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
#define CAM_H 260

namespace ui {
namespace home {

static lv_obj_t * screen = nullptr;
static lv_obj_t * img_camera = nullptr;
static lv_obj_t * lbl_time = nullptr;// 时间显示标签
static lv_obj_t * lbl_date = nullptr; //日期标签
static lv_obj_t * lbl_disk_warn = nullptr;// 磁盘满警告标签
static lv_obj_t * lbl_hint = nullptr;// 底部提示标签
static lv_timer_t * timer_cam = nullptr;// 摄像头刷新定时器

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
 * @brief 屏幕销毁时的清理回调
 * @details 必须手动将所有静态指针置空，否则后台异步事件会访问野指针导致崩溃
 */
static void screen_cleanup_cb(lv_event_t * e) {
    // 1. 删除摄像头刷新定时器
    if (timer_cam) {
        lv_timer_del(timer_cam);
        timer_cam = nullptr;
    }

    // 2. 将所有子控件指针置空
    lbl_time = nullptr;
    lbl_disk_warn = nullptr;
    img_camera = nullptr;
    lbl_hint = nullptr;
    lbl_date = nullptr;
    
    // screen 指针由 UiManager 管理，但在这里置空也无妨
    screen = nullptr;

    printf("[Home] Resources cleaned up safely.\n");
}

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
            ui::menu::load_menu_screen(); 
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

// 辅助函数：获取当前格式化日期 (YYYY-MM-DD)
static std::string get_current_date() {
    std::time_t now = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&now));
    return std::string(buf);
}

// 创建并配置屏幕
static void create_screen() {
    if (screen){
        lv_obj_delete(screen);
        screen = nullptr;
    }

    // 1. 使用标准框架构建 (自动应用深蓝渐变背景 + 玻璃质感 Header/Footer)
    BaseScreenParts parts = create_base_screen(""); 
    screen = parts.screen; // 将创建好的标准屏幕赋值给全局变量


    // ============================================================
    // 定制 Header 布局 (左:日期 | 中:Hello | 右:时间)
    // ============================================================

    // A. 清理 Header
    // create_base_screen 默认会创建 Title 和 Time，我们不需要默认的，直接清空
    lv_obj_clean(parts.header); 

    UiManager::getInstance()->registerScreen(ScreenType::MAIN, &screen);

    // 绑定销毁回调
    lv_obj_add_event_cb(screen, [](lv_event_t * e) {
        screen = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 设置 Flex 布局：两端对齐
    lv_obj_set_flex_flow(parts.header, LV_FLEX_FLOW_ROW); // 行布局
    // SPACE_BETWEEN 会自动把第一个元素推到最左，第二个推到最右
    lv_obj_set_flex_align(parts.header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 这里设置 10px，意味着左边的标签离左边 10px，右边的标签离右边 10px
    lv_obj_set_style_pad_left(parts.header, 10, 0);
    lv_obj_set_style_pad_right(parts.header, 10, 0);
    lv_obj_set_style_pad_top(parts.header, 0, 0);
    lv_obj_set_style_pad_bottom(parts.header, 0, 0);

    // B. 左侧：日期 (Year-Month-Day)
    lv_obj_t* obj_date = lv_label_create(parts.header);
    lv_label_set_text(obj_date, get_current_date().c_str()); 
    lv_obj_set_style_text_color(obj_date, lv_color_white(), 0);
    lv_obj_set_style_text_font(obj_date, &lv_font_montserrat_14, 0); // 稍微小一点的字体
    lv_obj_align(obj_date, LV_ALIGN_LEFT_MID, 10, 0); // 左边距 10

    // C. 中间：Hello 文字
    lv_obj_t * lbl_center = lv_label_create(parts.header);
    lv_label_set_text(lbl_center, "Hello"); // 或者 "Face Rec"
    lv_obj_add_style(lbl_center, &style_text_cn, 0); // 使用你的中文字体样式
    lv_obj_set_style_text_color(lbl_center, lv_color_white(), 0);
    lv_obj_align(lbl_center, LV_ALIGN_CENTER, 0, 0); // 绝对居中

    // D. 右侧：时间 (HH:MM:SS)
    lbl_time = lv_label_create(parts.header); 
    lv_label_set_text(lbl_time, "00:00"); // 初始占位符
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_time, LV_ALIGN_RIGHT_MID, -10, 0); // 右边距 -10 (向左缩进)

    // 2. 配置中间内容区 (放置摄像头)
    // 我们的摄像头分辨率是 240x260 (CAM_W x CAM_H)
    // 屏幕宽是 240，所以摄像头刚好撑满宽度
    // 确保 content 没有内边距，以便摄像头画面能贴边显示
    lv_obj_set_style_pad_all(parts.content, 0, 0);// 让摄像头画面能贴边显示

    // 创建摄像头图像对象，父对象直接设为 parts.content
    img_dsc.data = UiManager::getInstance()->getCameraDisplayBuffer();
    img_camera = lv_image_create(parts.content);
    lv_image_set_src(img_camera, &img_dsc);
    lv_obj_center(img_camera);// 让摄像头画面居中显示在中间区域

    // 给摄像头加一个简单的边框，增加一点科技感
    lv_obj_set_style_border_width(img_camera, 1, 0);
    lv_obj_set_style_border_color(img_camera, lv_color_hex(0x66CCFF), 0);
    lv_obj_set_style_border_side(img_camera, (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM), 0);

    // 3. 自定义 Header 内容 (覆盖/添加标准内容)

    // 底部提示语 (覆盖默认的 "退出-ESC 确认-ENTER")
    set_base_footer_hint(parts.footer, LV_SYMBOL_POWER " -ESC ",LV_SYMBOL_HOME " -ENTER "); // 设置底部提示语

    lv_obj_t* lbl_left = lv_obj_get_child(parts.footer, 0);  // 获取左边的 Label (退出提示)
    lv_obj_t* lbl_right = lv_obj_get_child(parts.footer, 1); // 获取右边的 Label (确认提示)
    
    if (lbl_left) {
        lv_obj_set_style_text_font(lbl_left, &lv_font_montserrat_14, 0);
    }
    if (lbl_right) {
        lv_obj_set_style_text_font(lbl_right, &lv_font_montserrat_14, 0);
    }

    // 5. 事件与逻辑绑定
    // 绑定按键事件 (挂在最外层 screen 上)
    lv_obj_add_event_cb(screen, screen_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(screen, screen_cleanup_cb, LV_EVENT_DELETE, NULL);// 绑定销毁清理回调，确保资源安全释放
    
}

// 加载屏幕
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
            std::string* time_str_ptr = (std::string*)d;
                if(lbl_time && time_str_ptr && !time_str_ptr->empty()) {
                lv_label_set_text(lbl_time, time_str_ptr->c_str());
            }
        }, new std::string(*t));
    });

    bus.subscribe(EventType::DISK_FULL, [](void*){
        lv_async_call([](void*){ if(lbl_disk_warn) lv_obj_remove_flag(lbl_disk_warn, LV_OBJ_FLAG_HIDDEN); }, nullptr);
    });

    // 启动定时器刷新摄像头
    if (!timer_cam) {
        timer_cam = lv_timer_create(timer_cam_cb, 33, nullptr);
    }

    lv_screen_load(screen);
    UiManager::getInstance()->destroyAllScreensExcept(screen);
}

} // namespace home
} // namespace ui
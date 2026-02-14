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

static lv_obj_t *scr_sys = nullptr;
static lv_obj_t *scr_param = nullptr;
static lv_obj_t *scr_basic = nullptr;//基础设置屏幕
static lv_obj_t *scr_advanced = nullptr;//高级设置屏幕


static struct {
    // 基础设置
    int screen_timeout = 30;        // 返回主界面时间(秒)
    int admin_count = 10;           // 管理员总数
    int record_warning = 99;        // 记录预警数
    int screen_saver = 15;          // 屏幕保护时间(分钟)
    int volume = 5;                // 音量(0-10)
    int machine_id = 1;            // 设备ID
    char date_format[16] = "YYYY-MM-DD";  // 日期格式
    char time_str[16] = "12:00";          // 时间格式
    char language[16] = "zh-CN";          // 语言
    
    // 高级设置参数
    bool initialized = false;      // 是否已初始化
} g_config;

// 日期格式选项
static const char* date_format_options[] = {
    "YYYY-MM-DD",
    "DD/MM/YYYY",
    "MM/DD/YYYY",
    "YYYY.MM.DD",
    NULL
};

// 语言选项
static const char* language_options[] = {
    "中文",
    "English",
    NULL
};

//工具函数
namespace {
    void save_config() {
        // 这里可以实现将 g_config 保存到文件或持久化存储的逻辑
        printf("配置已保存：超时=%d, 音量=%d, 设备ID=%d\n", g_config.screen_timeout, g_config.volume, g_config.machine_id);
    }

// 加载配置，确保只加载一次
void load_config() {
    if (!g_config.initialized) {
        g_config.initialized = true;
        // 这里可以实现从文件或持久化存储加载 g_config 的逻辑
    }
}

//数字输入键盘回调
struct NumberInputContext {
        int* target_value; // 指向要修改的配置项
        int min_value;     // 最小值
        int max_value;     // 最大值
        lv_obj_t* parent_screen; // 父屏幕指针
        const char* setting_name; // 设置项名称
    };

// 数字输入事件回调
static void number_input_cb(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        NumberInputContext* ctx = (NumberInputContext*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        // 处理数字输入
        if (key >= '0' && key <= '9'){
            int digit = key - '0';
            int current = *(ctx->target_value);
            int new_value = current * 10 + digit;
            if (new_value <= ctx->max_value) {
                    *(ctx->target_value) = new_value;
        }
    }

    else if (key == LV_KEY_BACKSPACE) {
            // 处理删除键
            *(ctx->target_value) /= 10;
        }

    else if (key == LV_KEY_ENTER) {
            // 确认输入，保存配置并返回
            save_config();
            char msg[64];
            snprintf(msg, sizeof(msg), "%s 已设置为 %d", ctx->setting_name, *(ctx->target_value));
            ::show_popup(ctx->setting_name, msg);
            // 返回上一级
            lv_screen_load(ctx->parent_screen);
            delete ctx; // 释放上下文内存
        }
            
    else if (key == LV_KEY_ESC) {
            // 取消输入，返回不保存
            lv_screen_load(ctx->parent_screen);
             delete ctx; // 释放上下文内存
        }
    }
}

//显示数据输入界面
void show_number_input(lv_obj_t* parent, const char* title, int* value, int min_val, int max_val) {
    // 创建输入屏幕
    lv_obj_t* input_scr = lv_obj_create(nullptr);
    lv_obj_add_style(input_scr, &style_base, 0);

    // 标题
    lv_obj_t* title_label = lv_label_create(input_scr);
    char title_buf[32];
    snprintf(title_buf, sizeof(title_buf), "%s 设置", title);
    lv_label_set_text(title_label, title_buf);
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // 显示当前值
    lv_obj_t* value_label = lv_label_create(input_scr);
    lv_label_set_text_fmt(value_label, "当前值：%d", *value);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_20, 0);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -30);

    //提示信息
    lv_obj_t* hint = lv_label_create(input_scr);
    lv_label_set_text_fmt(hint, "输入范围：%d - %d\n按数字键输入，ENTER确认，ESC取消", min_val, max_val);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -30);

    // 创建上下文并保存到事件中
    NumberInputContext* ctx = new NumberInputContext;
    ctx->target_value = value;
    ctx->min_value = min_val;
    ctx->max_value = max_val;
    ctx->parent_screen = parent;
    ctx->setting_name = title;

    //添加键盘事件
    lv_obj_add_event_cb(input_scr, number_input_cb, LV_EVENT_KEY, ctx);

    //添加到焦点组
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(input_scr);
    lv_screen_load(input_scr);
}

//选项选择回调
struct OptionSelectContext {
    char* target_value; // 指向要修改的配置项
    size_t target_size; // 配置项大小（字节）
    const char** options; // 选项列表
    lv_obj_t* parent_screen; // 父屏幕指针
    const char* setting_name; // 设置项名称
    lv_obj_t* preview_label; // 预览标签指针
};

//选项选择事件回调
static void option_select_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    OptionSelectContext* ctx = (OptionSelectContext*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {
        // 获取选项索引
        const char* selected  = (const char*)lv_obj_get_user_data(btn);
        char preview_buf[64];
        snprintf(preview_buf, sizeof(preview_buf), "当前选择：%s", selected);
        lv_label_set_text(ctx->preview_label, preview_buf);

        // 更新配置项值
        strncpy(ctx->target_value, selected, ctx->target_size-1);
        ctx->target_value[ctx->target_size-1] = '\0'; // 确保字符串以空字符结尾
    }
}

//选项选择确认回调
static void option_confirm_cb(lv_event_t* e){
    OptionSelectContext* ctx = (OptionSelectContext*)lv_event_get_user_data(e);

    if(lv_event_get_code(e) == LV_EVENT_CLICKED || (lv_event_get_code(e) == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {

        save_config();
        char msg[64];
        snprintf(msg, sizeof(msg), "%s 已设置为 %s", ctx->setting_name, ctx->target_value);
        ::show_popup(ctx->setting_name, msg);
        
        // 返回上一级
        lv_screen_load(ctx->parent_screen);
        delete ctx; // 释放上下文内存
    }
    else if(lv_event_get_code(e) == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        // 取消选择，返回不保存
        lv_screen_load(ctx->parent_screen);
        delete ctx; // 释放上下文内存
    }
}

//显示选项选择界面
void show_option_select(lv_obj_t* parent, const char* title, char* target, size_t target_size, const char** options) {
    // 创建选项屏幕
    lv_obj_t* option_scr = lv_obj_create(nullptr);
    lv_obj_add_style(option_scr, &style_base, 0);

    // 标题
    lv_obj_t* title_label = lv_label_create(option_scr);
    char title_buf[32];
    snprintf(title_buf, sizeof(title_buf), "%s 选择", title);
    lv_label_set_text(title_label, title_buf);
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // 当前值预览
    lv_obj_t* preview = lv_label_create(option_scr);
    lv_label_set_text_fmt(preview, "当前选择：%s", target);
    lv_obj_align(preview, LV_ALIGN_TOP_MID, 0, 50);

    //选择列表容器
    lv_obj_t* list = lv_obj_create(option_scr);
    lv_obj_set_size(list, 200, 180);
    lv_obj_center(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);

    //创建上下文
    OptionSelectContext* ctx = new OptionSelectContext;
    ctx->target_value = target;
    ctx->target_size = target_size;
    ctx->options = options;
    ctx->parent_screen = parent;
    ctx->setting_name = title;
    ctx->preview_label = preview;

    // 创建选项按钮
    std::vector<lv_obj_t*> option_buttons;
    for(size_t i = 0; options[i] != nullptr; i++) {
        // 创建按钮
        lv_obj_t* btn = lv_btn_create(list);
        lv_obj_set_width(btn, 180);
        lv_obj_set_height(btn, 40);

        // 设置按钮文本
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, options[i]);
        lv_obj_center(label);

        lv_obj_set_user_data(btn, (void*)options[i]); // 将选项文本作为用户数据保存到按钮
        lv_obj_add_event_cb(btn, option_select_cb, LV_EVENT_ALL, ctx);

        option_buttons.push_back(btn);
    }

    // 确认按钮
    lv_obj_t* confirm_btn = lv_btn_create(option_scr);
    lv_obj_set_size(confirm_btn, 100, 40);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(confirm_btn, lv_palette_main(LV_PALETTE_GREEN), 0);

    lv_obj_t* confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, "确认");
    lv_obj_center(confirm_label);

    lv_obj_add_event_cb(confirm_btn, option_confirm_cb, LV_EVENT_ALL, ctx);
    option_buttons.push_back(confirm_btn);

    //添加到焦点组
    UiManager::getInstance()->resetKeypadGroup();
    for(auto btn : option_buttons) {
        UiManager::getInstance()->addObjToGroup(btn);
    }
    lv_group_focus_obj(option_buttons[0]); // 默认焦点在第一个选项

    //ESC键返回上一级
    lv_obj_add_event_cb(option_scr, [](lv_event_t* e){
        if(lv_event_get_code(e) == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
            OptionSelectContext* ctx = (OptionSelectContext*)lv_event_get_user_data(e);
            lv_screen_load(ctx->parent_screen);
            delete ctx; // 释放上下文内存
        }
    }, LV_EVENT_KEY, ctx);

    UiManager::getInstance()->addObjToGroup(option_scr);
    lv_screen_load(option_scr);
}

//音量调节模块界面
void show_volume_slider(lv_obj_t* parent){
    // 创建滑动条屏幕
    lv_obj_t* slider_scr = lv_obj_create(nullptr);
    lv_obj_add_style(slider_scr, &style_base, 0);

    // 标题
    lv_obj_t* title_label = lv_label_create(slider_scr);
    lv_label_set_text(title_label, "音量设置");
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // 滑动条
    lv_obj_t* slider = lv_slider_create(slider_scr);
    lv_slider_set_range(slider, 200, 20);
    lv_obj_center(slider);
    lv_slider_set_range(slider, 0, 10);
    lv_slider_set_value(slider, g_config.volume, LV_ANIM_OFF);

    //音量数值显示
    lv_obj_t* value_label = lv_label_create(slider_scr);
    lv_label_set_text_fmt(value_label, "当前音量：%d", g_config.volume);
    lv_obj_align(value_label, LV_ALIGN_OUT_TOP_MID, 0, -20);

    // 滑动条事件回调
    lv_obj_add_event_cb(slider, [](lv_event_t* e){
        auto slider = lv_event_get_target(e);
        lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);
        int value = (int)lv_slider_get_value(static_cast<const lv_obj_t*>(slider));
        lv_label_set_text_fmt(label, "当前音量：%d", value);
    }, LV_EVENT_VALUE_CHANGED, value_label);

    //确认按钮
    lv_obj_t* confirm_btn = lv_btn_create(slider_scr);
    lv_obj_set_size(confirm_btn, 100, 40);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_t* confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, "保存");
    lv_obj_center(confirm_label);

    lv_obj_add_event_cb(confirm_btn, [](lv_event_t* e){
        lv_obj_t* slider = (lv_obj_t*)lv_event_get_user_data(e);
        g_config.volume = (int)lv_slider_get_value(slider);
        save_config();
        char msg[64];
        snprintf(msg, sizeof(msg), "音量已设置为 %d", g_config.volume);
        ::show_popup("音量设置", msg);
        lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
        lv_screen_load(lv_obj_get_parent(target));
    }, LV_EVENT_CLICKED, slider);
    lv_obj_set_user_data(confirm_btn, parent);

    //添加到焦点组
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(slider);
    UiManager::getInstance()->addObjToGroup(confirm_btn);
    lv_group_focus_obj(slider); // 默认焦点在滑动条

//ESC键返回上一级
lv_obj_add_event_cb(slider_scr, [](lv_event_t* e) {
            if (lv_event_get_key(e) == LV_KEY_ESC) {
                lv_obj_t* parent = (lv_obj_t*)lv_event_get_user_data(e);
                lv_screen_load(parent);
            }
        }, LV_EVENT_KEY, parent);

        UiManager::getInstance()->addObjToGroup(slider_scr);
        
        lv_screen_load(slider_scr);
    }

//时间设置界面
    void show_time_setting(lv_obj_t* parent) {
        // 简化的时间设置：直接输入小时分钟
        show_number_input(parent, "时间", (int*)&g_config.time_str[0], 0, 2359);
    }
}

// =================基础设置界面=================
//基础设置事件回调
static void basic_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        //导航逻辑
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + total - 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

        else if (key == LV_KEY_ESC) {
            load_sys_settings_menu_screen(); // 返回上一级
        }

        else if (key == LV_KEY_ENTER) {
            
            if (strcmp(tag, "TIME") == 0) {
                // 时间设置 - 数字输入
                show_time_setting(scr_basic);
            }

            else if (strcmp(tag, "DATE_FORMAT") == 0) {
                // 日期格式 - 选项选择
                show_option_select(scr_basic, "日期格式", g_config.date_format, sizeof(g_config.date_format), date_format_options);
            }

            else if (strcmp(tag, "VOLUME") == 0) {
                // 音量设置 - 滑块调节
                show_volume_slider(scr_basic);
            }

            else if (strcmp(tag, "LANGUAGE") == 0) {
                // 语言设置 - 选项选择
                show_option_select(scr_basic, "语言", g_config.language, sizeof(g_config.language), language_options);
            }

            else if (strcmp(tag, "SCREEN_SAVER") == 0) {
                // 屏保时间 - 数字输入 (0-60分钟)
                show_number_input(scr_basic, "屏保时间",  &g_config.screen_saver, 0, 60);
            }

            else if (strcmp(tag, "MACHINE_ID") == 0) {
                // 机器号设置 - 数字输入 (1-999)
                show_number_input(scr_basic, "设备ID", &g_config.machine_id, 1, 999);
            }

            else if (strcmp(tag, "SCREEN_TIMEOUT") == 0) {
                // 返回主界面时间 - 数字输入 (10-300秒)
                show_number_input(scr_basic, "返回主界面时间", &g_config.screen_timeout, 10, 300);
            }
            else if (strcmp(tag, "ADMIN_COUNT") == 0) {
                // 管理员总数 - 数字输入 (1-100)
                show_number_input(scr_basic, "管理员总数", &g_config.admin_count, 1, 100);
            }
            else if (strcmp(tag, "RECORD_WARNING") == 0) {
                // 记录警告数 - 数字输入 (10-999)
                show_number_input(scr_basic, "记录警告数", &g_config.record_warning, 10, 999);
            }
        }
    }
}

//加载基础设置屏幕实现
void load_sys_settings_basic_screen() {
    // 确保配置已加载
    load_config();
    
    // 创建基础设置屏幕
    if (scr_basic) lv_obj_delete(scr_basic);
    scr_basic = lv_obj_create(nullptr);
    lv_obj_add_style(scr_basic, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_SETTINGS, &scr_basic);

    // 标题
    lv_obj_t *title = lv_label_create(scr_basic);
    lv_label_set_text(title, "基础设置 / Basic Settings");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 滚动容器
    lv_obj_t *scroll = lv_obj_create(scr_basic);
    lv_obj_set_size(scroll, 240, 280);
    lv_obj_center(scroll);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scroll, 8, 0);

    // 基础设置菜单项
    struct {
        const char* icon;
        const char* en_text;
        const char* cn_text;
        const char* tag;
        const char* current_value;
    } basic_items[] = {
        {LV_SYMBOL_SETTINGS, "Time", "时间设置", "TIME", g_config.time_str},
        {LV_SYMBOL_TINT, "Date Format", "日期格式", "DATE_FORMAT", g_config.date_format},
        {LV_SYMBOL_VOLUME_MID, "Volume", "音量", "VOLUME", nullptr},
        {LV_SYMBOL_CHARGE, "Language", "语言", "LANGUAGE", nullptr},
        {LV_SYMBOL_POWER, "Screen Saver", "屏幕保护", "SCREEN_SAVER", nullptr},
        {LV_SYMBOL_EYE_OPEN, "Machine ID", "设备ID", "MACHINE_ID", nullptr},
        {LV_SYMBOL_CLOSE, "Screen Timeout", "返回主界面时间", "SCREEN_TIMEOUT", nullptr},
        {LV_SYMBOL_DRIVE, "Admin Count", "管理员总数", "ADMIN_COUNT", nullptr},
        {LV_SYMBOL_WARNING, "Record Warning", "记录警告数", "RECORD_WARNING", nullptr}
    };

    // 创建带当前值显示的按钮
    std::vector<lv_obj_t*> buttons;
    for (int i = 0; i < 9; i++) {
        // 创建主按钮
        lv_obj_t *btn = ::create_sys_grid_btn(scroll, i, basic_items[i].icon, basic_items[i].en_text, basic_items[i].cn_text, basic_event_cb,  basic_items[i].tag);
        lv_obj_set_width(btn, 220);
        
        // 添加当前值标签
        char value_str[32] = {0};
        switch(i) {
            case 2: // 音量
                snprintf(value_str, sizeof(value_str), "%d/10", g_config.volume);
                break;
            case 3: // 语言
                snprintf(value_str, sizeof(value_str), "%s", 
                        strcmp(g_config.language, "zh-CN") == 0 ? "中文" : "English");
                break;
            case 4: // 屏保时间
                snprintf(value_str, sizeof(value_str), "%d分钟", g_config.screen_saver);
                break;
            case 5: // 设备ID
                snprintf(value_str, sizeof(value_str), "%d", g_config.machine_id);
                break;
            case 6: // 返回主界面时间
                snprintf(value_str, sizeof(value_str), "%d秒", g_config.screen_timeout);
                break;
            case 7: // 管理员总数
                snprintf(value_str, sizeof(value_str), "%d", g_config.admin_count);
                break;
            case 8: // 记录警告数
                snprintf(value_str, sizeof(value_str), "%d", g_config.record_warning);
                break;
            default:
                if (basic_items[i].current_value) {
                    strncpy(value_str, basic_items[i].current_value, sizeof(value_str)-1);
                }
        }
        
        if (strlen(value_str) > 0) {
            lv_obj_t* value_label = lv_label_create(btn);
            lv_label_set_text(value_label, value_str);
            lv_obj_set_style_text_color(value_label, lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -10, 0);
        }
        
        buttons.push_back(btn);
    }

    // 焦点组设置
    UiManager::getInstance()->resetKeypadGroup();
    for (auto btn : buttons) {
        UiManager::getInstance()->addObjToGroup(btn);
    }
    if (!buttons.empty()) {
        lv_group_focus_obj(buttons[0]);
    }

    // ESC键处理
    lv_obj_add_event_cb(scr_basic, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) {
            load_sys_settings_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_basic);

    lv_screen_load(scr_basic);
    UiManager::getInstance()->destroyAllScreensExcept(scr_basic);
}

// ================= 高级设置===============
// 确认对话框上下文结构
struct ConfirmDialogContext {
    const char* operation_tag;     // 操作标签
    lv_obj_t* parent_screen;       // 父屏幕指针
    const char* operation_name;    // 操作名称
};

// 确认对话框事件回调
static void confirm_dialog_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    ConfirmDialogContext* ctx = (ConfirmDialogContext*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const char* choice = (const char*)lv_obj_get_user_data(btn);

    if (code == LV_EVENT_CLICKED || 
        (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER)) {
        
        if (strcmp(choice, "YES") == 0) {
            // 显示工作中提示
            ::show_popup(ctx->operation_name, "工作中，请稍后...");
            
            // 根据操作类型执行不同的清除逻辑
            if (strcmp(ctx->operation_tag, "CLEAR_RECORDS") == 0) {
                // 清除所有记录 - 删除考勤记录，保留员工数据和系统设置
                UiController::getInstance()->clearAllRecords();
                ::show_popup(ctx->operation_name, "考勤记录已清除\n删除成功");
            }
            else if (strcmp(ctx->operation_tag, "CLEAR_EMPLOYEES") == 0) {
                // 清除所有员工 - 删除员工数据，保留考勤记录和系统设置
                UiController::getInstance()->clearAllEmployees();
                ::show_popup(ctx->operation_name, "员工数据已清除\n删除成功");
            }
            else if (strcmp(ctx->operation_tag, "CLEAR_ALL_DATA") == 0) {
                // 清除所有数据 - 删除员工数据和考勤记录，保留系统设置
                UiController::getInstance()->clearAllData();
                ::show_popup(ctx->operation_name, "所有数据已清除\n删除成功");
            }
            else if (strcmp(ctx->operation_tag, "FACTORY_RESET") == 0) {
                // 恢复出厂设置
                UiController::getInstance()->factoryReset();
                ::show_popup("恢复出厂设置", "恢复出厂设置成功\n系统将重启");
                
                // 延时重启
                lv_timer_t *t = lv_timer_create([](lv_timer_t*){ exit(0); }, 3000, nullptr);
                lv_timer_set_repeat_count(t, 1);
                return;  // 不返回上一级，等待重启
            }
            else if (strcmp(ctx->operation_tag, "SYSTEM_UPDATE") == 0) {
                // 系统更新
                ::show_popup("系统升级", "正在升级...\n请勿断电");
                // 实际升级逻辑会在UiController中实现
            }
        }
        
        // 返回高级设置菜单（除了恢复出厂设置）
        if (strcmp(ctx->operation_tag, "FACTORY_RESET") != 0) {
            lv_screen_load(ctx->parent_screen);
        }
        delete ctx;
    }
}

// ESC键处理函数
static void dialog_esc_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY && 
        lv_event_get_key(e) == LV_KEY_ESC) {
        ConfirmDialogContext* ctx = (ConfirmDialogContext*)lv_event_get_user_data(e);
        lv_screen_load(ctx->parent_screen);
        delete ctx;
    }
}

// 显示确认对话框
static void show_confirm_dialog(lv_obj_t* parent, const char* title, const char* message, const char* tag) {
    // 创建对话框屏幕
    lv_obj_t* dialog_scr = lv_obj_create(nullptr);
    lv_obj_add_style(dialog_scr, &style_base, 0);
    
    // 标题
    lv_obj_t* title_label = lv_label_create(dialog_scr);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // 消息内容
    lv_obj_t* msg_label = lv_label_create(dialog_scr);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
    lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, -20);
    
    // 创建上下文
    ConfirmDialogContext* ctx = new ConfirmDialogContext;
    ctx->operation_tag = tag;
    ctx->parent_screen = parent;
    ctx->operation_name = title;
    
    // 按钮容器
    lv_obj_t* btn_container = lv_obj_create(dialog_scr);
    lv_obj_set_size(btn_container, 200, 60);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    
    // 是/确认按钮
    lv_obj_t* yes_btn = lv_btn_create(btn_container);
    lv_obj_set_size(yes_btn, 80, 40);
    lv_obj_set_style_bg_color(yes_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    
    lv_obj_t* yes_label = lv_label_create(yes_btn);
    lv_label_set_text(yes_label, "是");
    lv_obj_center(yes_label);
    
    lv_obj_set_user_data(yes_btn, (void*)"YES");
    lv_obj_add_event_cb(yes_btn, confirm_dialog_cb, LV_EVENT_ALL, ctx);
    
    // 否/取消按钮
    lv_obj_t* no_btn = lv_btn_create(btn_container);
    lv_obj_set_size(no_btn, 80, 40);
    lv_obj_set_style_bg_color(no_btn, lv_palette_main(LV_PALETTE_RED), 0);
    
    lv_obj_t* no_label = lv_label_create(no_btn);
    lv_label_set_text(no_label, "否");
    lv_obj_center(no_label);
    
    lv_obj_set_user_data(no_btn, (void*)"NO");
    lv_obj_add_event_cb(no_btn, confirm_dialog_cb, LV_EVENT_ALL, ctx);
    
    // 提示信息
    lv_obj_t* hint = lv_label_create(dialog_scr);
    lv_label_set_text(hint, "退出-ESC  确认-OK");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);
    
    // 焦点组设置
    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(yes_btn);
    UiManager::getInstance()->addObjToGroup(no_btn);
    lv_group_focus_obj(yes_btn);
    
    // ESC键处理
    lv_obj_add_event_cb(dialog_scr, dialog_esc_cb, LV_EVENT_KEY, ctx);
    UiManager::getInstance()->addObjToGroup(dialog_scr);
    
    lv_screen_load(dialog_scr);
}

//高级设置事件回调
static void advanced_setting_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        //导航逻辑
        if(key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + total - 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

        else if (key == LV_KEY_ESC) {
            load_sys_settings_menu_screen(); // 返回上一级
        }

        else if (key == LV_KEY_ENTER) {
            // 显示确认对话框
            if(strcmp(tag, "CLEAR_RECORDS") == 0) {
                show_confirm_dialog(scr_advanced, "清除所有记录", "确定清除所有考勤记录吗？\n保留员工数据和系统设置", "CLEAR_RECORDS");
            }
            else if (strcmp(tag, "CLEAR_EMPLOYEES") == 0) {
                show_confirm_dialog(scr_advanced, "清除所有员工", "确定清除所有员工数据吗？\n保留考勤记录和系统设置", "CLEAR_EMPLOYEES");
            }
            else if (strcmp(tag, "CLEAR_ALL_DATA") == 0) {
                show_confirm_dialog(scr_advanced, "清除所有数据", "确定清除所有员工数据和考勤记录吗？\n保留系统设置", "CLEAR_ALL_DATA");
            }
            else if (strcmp(tag, "FACTORY_RESET") == 0) {
                show_confirm_dialog(scr_advanced, "恢复出厂设置", "确定恢复出厂设置吗？\n所有数据将被清除,系统将重启", "FACTORY_RESET");
            }
            else if (strcmp(tag, "SYSTEM_UPDATE") == 0) {
                show_confirm_dialog(scr_advanced, "系统升级","插入U盘进行系统升级\n按「ENTER」开始升级","SYSTEM_UPDATE");
            }
        }
    }
}

// 加载高级设置屏幕
void load_system_advanced_screen(){
    if (scr_advanced)lv_obj_delete(scr_advanced);
    scr_advanced = lv_obj_create(nullptr);
    lv_obj_add_style(scr_advanced, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADVANCED, &scr_advanced);
   
    lv_obj_t *title = lv_label_create(scr_advanced);
    lv_label_set_text(title, "高级设置 / Advanced Settings");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    //Grid布局
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, 50, 50, 50, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(scr_advanced);
    lv_obj_set_size(grid, 220, 300);
    lv_obj_center(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);

    //创建5个设置项按钮
    const char* icons[] = {LV_SYMBOL_TRASH, LV_SYMBOL_CLOSE, LV_SYMBOL_WARNING, LV_SYMBOL_REFRESH, LV_SYMBOL_DOWNLOAD}; // 5个图标
    const char* tags[] = {"CLEAR_RECORDS", "CLEAR_EMPLOYEES", "CLEAR_ALL_DATA", "FACTORY_RESET", "SYSTEM_UPDATE"}; // 5个标签
    const char* en_texts[] = {"Clear Records", "Clear Employers", "Clear All Data", "Factory Reset", "System Update"}; // 5个英文文本
    const char* cn_texts[] = {"清除所有记录", "清除所有员工信息", "清除所有数据", "恢复出厂设置", "系统更新"}; // 5个中文文本

    for(int i = 0; i < 5; i++) {
        lv_obj_t *btn = ::create_sys_grid_btn(grid, i, icons[i], en_texts[i], cn_texts[i], advanced_setting_event_cb, tags[i]);

        if(i < 4){
            lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), 0); // 前4个按钮红色背景
        }
    }

    // 添加底部提示
    lv_obj_t* hint = lv_label_create(scr_advanced);
    lv_label_set_text(hint, "退出-ESC  确认-OK");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);

    UiManager::getInstance()->resetKeypadGroup();
    for(int i = 0; i < 5; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(grid, i));
    }
    lv_group_focus_obj(lv_obj_get_child(grid, 0));

    lv_obj_add_event_cb(scr_advanced, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_sys_settings_menu_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_advanced);

    lv_screen_load(scr_advanced);
    UiManager::getInstance()->destroyAllScreensExcept(scr_advanced);
}

// ================= 参数设置==========

// 菜单按钮事件回调
static void param_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + total - 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }
        else if (key == LV_KEY_ESC) {
            load_sys_settings_menu_screen(); // 返回上一级
        }
        else if (key == LV_KEY_ENTER) {
            if (strcmp(tag, "THRESHOLD") == 0) {
                // 模拟切换阈值
                show_popup("Threshold", "Value: 0.75 (Fixed)");
            }
            else if (strcmp(tag, "ROI") == 0) {
                // 模拟切换 ROI
                show_popup("ROI", "ROI Crop: ON");
            }
        }
    }
}

// 加载参数设置屏幕实现
void load_system_param_screen() {
    if (scr_param) lv_obj_delete(scr_param);
    scr_param = lv_obj_create(nullptr);
    lv_obj_add_style(scr_param, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADVANCED, &scr_param);

    lv_obj_t *title = lv_label_create(scr_param);
    lv_label_set_text(title, "参数设置 / Params");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_t *grid = lv_obj_create(scr_param);
    lv_obj_set_size(grid, 220, 150);
    lv_obj_center(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    // 仅使用基础图标
    lv_obj_t *b1 = create_sys_grid_btn(grid, 0, LV_SYMBOL_SETTINGS, "Threshold", "识别阈值", param_event_cb, "THRESHOLD");
    lv_obj_t *b2 = create_sys_grid_btn(grid, 1, LV_SYMBOL_EYE_OPEN, "ROI Crop", "ROI裁剪", param_event_cb, "ROI");

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(b1);
    UiManager::getInstance()->addObjToGroup(b2);
    lv_group_focus_obj(b1);

    lv_obj_add_event_cb(scr_param, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_sys_settings_menu_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_param);

    lv_screen_load(scr_param);
    UiManager::getInstance()->destroyAllScreensExcept(scr_param);
}

//自检功能界面
static void self_check_menu_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    //
    if (code == LV_EVENT_KEY){
        uint32_t key = lv_event_get_key(e);

        // 导航逻辑
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

        // 导航逻辑
        else if(key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + total - 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

        // ESC键返回上一级
        else if (key == LV_KEY_ESC) {
            load_sys_settings_menu_screen(); // 返回上一级
        }

        // 确认键执行对应的自检功能
        else if (key == LV_KEY_ENTER) {
            // 指纹采集
            if(strcmp(tag, "Fingerprint") == 0) {
                ::show_popup("自检功能", "正在采集指纹...\n请稍后");
                // 模拟指纹采集过程
                lv_timer_t *t = lv_timer_create([](lv_timer_t*){
                    ::show_popup("自检功能", "指纹采集成功");
                }, 2000, nullptr);
                lv_timer_set_repeat_count(t, 1);
            }

            // 红外摄像头采集
            else if (strcmp(tag, "Camera") == 0) {
                ::show_popup("自检功能", "正在采集摄像头图像...\n请稍后");
                // 模拟摄像头采集过程
                lv_timer_t *t = lv_timer_create([](lv_timer_t*){
                    ::show_popup("自检功能", "摄像头采集成功");
                }, 2000, nullptr);
                lv_timer_set_repeat_count(t, 1);
            }

            //彩色摄像头采集
            else if (strcmp(tag, "Color Camera") == 0) {
                ::show_popup("自检功能", "正在采集彩色摄像头图像...\n请稍后");
                // 模拟彩色摄像头采集过程
                lv_timer_t *t = lv_timer_create([](lv_timer_t*){
                    ::show_popup("自检功能", "彩色摄像头采集成功");
                }, 2000, nullptr);
                lv_timer_set_repeat_count(t, 1);
            }     
        }
    }
}

// 加载自检主菜单屏幕
void load_self_check_menu_screen() {
    static lv_obj_t* scr_self_check = nullptr;
    if (scr_self_check) lv_obj_delete(scr_self_check);
    scr_self_check = lv_obj_create(nullptr);
    lv_obj_add_style(scr_self_check, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_SELF_CHECK, &scr_self_check);

    // 标题
    lv_obj_t* title = lv_label_create(scr_self_check);
    lv_label_set_text(title, "自检功能 / Self-checking");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Grid布局
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, 50, LV_GRID_TEMPLATE_LAST};

    lv_obj_t* grid = lv_obj_create(scr_self_check);
    lv_obj_set_size(grid, 220, 200);
    lv_obj_center(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);

    // 三个选项：指纹采集器、红外摄像头、彩色摄像头
    const char* icons[] = {LV_SYMBOL_TINT, LV_SYMBOL_IMAGE, LV_SYMBOL_IMAGE}; 
    const char* en_texts[] = {"Fingerprint", "IR Camera", "Color Camera"};
    const char* cn_texts[] = {"指纹采集器", "红外摄像头", "彩色摄像头"};
    const char* tags[] = {"FINGERPRINT", "IR_CAMERA", "COLOR_CAMERA"};

    std::vector<lv_obj_t*> buttons;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = ::create_sys_grid_btn(grid, i, icons[i], en_texts[i], cn_texts[i],
                                               self_check_menu_event_cb, tags[i]);
        buttons.push_back(btn);
    }

    // 底部提示
    lv_obj_t* hint = lv_label_create(scr_self_check);
    lv_label_set_text(hint, "退出-ESC  确认-OK");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);

    // 焦点组
    UiManager::getInstance()->resetKeypadGroup();
    for (auto btn : buttons) {
        UiManager::getInstance()->addObjToGroup(btn);
    }
    if (!buttons.empty()) {
        lv_group_focus_obj(buttons[0]);
    }

    lv_obj_add_event_cb(scr_self_check, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) load_sys_settings_menu_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_self_check);

    lv_screen_load(scr_self_check);
    UiManager::getInstance()->destroyAllScreensExcept(scr_self_check);
}

// 通用摄像头测试界面（红外/彩色）
static void load_camera_test_screen(const char* title, const char* type) {
    lv_obj_t* scr_cam = lv_obj_create(nullptr);
    lv_obj_add_style(scr_cam, &style_base, 0);

    // 标题
    lv_obj_t* title_label = lv_label_create(scr_cam);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // 图像显示区域（模拟，实际应调用摄像头驱动获取图像）
    lv_obj_t* img_placeholder = lv_obj_create(scr_cam);
    lv_obj_set_size(img_placeholder, 200, 150);
    lv_obj_center(scr_cam);
    lv_obj_set_style_bg_color(img_placeholder, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(img_placeholder, lv_color_white(), 0);
    lv_obj_set_style_border_width(img_placeholder, 2, 0);

    // 在占位区域显示文字提示
    lv_obj_t* label = lv_label_create(img_placeholder);
    lv_label_set_text(label, "摄像头图像\n(模拟)");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    // 提示信息
    lv_obj_t* hint = lv_label_create(scr_cam);
    lv_label_set_text(hint, "观察图像是否清晰可用\n退出-ESC");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ESC键返回自检菜单
    lv_obj_add_event_cb(scr_cam, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_self_check_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(scr_cam);
    lv_screen_load(scr_cam);
    UiManager::getInstance()->destroyAllScreensExcept(scr_cam);
}

// 红外摄像头自检
void load_ir_camera_test_screen() {
    load_camera_test_screen("红外摄像头", "IR");
}

// 彩色摄像头自检
void load_color_camera_test_screen() {
    load_camera_test_screen("彩色摄像头", "Color");
}

// 指纹采集器自检
void load_fingerprint_test_screen() {
    lv_obj_t* scr_fp = lv_obj_create(nullptr);
    lv_obj_add_style(scr_fp, &style_base, 0);

    // 标题
    lv_obj_t* title_label = lv_label_create(scr_fp);
    lv_label_set_text(title_label, "指纹采集器");
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // 指纹图像显示区域（模拟）
    lv_obj_t* fp_area = lv_obj_create(scr_fp);
    lv_obj_set_size(fp_area, 150, 150);
    lv_obj_center(scr_fp);
    lv_obj_set_style_bg_color(fp_area, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_color(fp_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(fp_area, 2, 0);

    lv_obj_t* fp_label = lv_label_create(fp_area);
    lv_label_set_text(fp_label, "请按压指纹");
    lv_obj_set_style_text_color(fp_label, lv_color_white(), 0);
    lv_obj_center(fp_label);

    // 提示信息
    lv_obj_t* hint = lv_label_create(scr_fp);
    lv_label_set_text(hint, "按压指纹查看图像是否清晰\n退出-ESC");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ESC键返回自检菜单
    lv_obj_add_event_cb(scr_fp, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_self_check_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(scr_fp);
    lv_screen_load(scr_fp);
    UiManager::getInstance()->destroyAllScreensExcept(scr_fp);
}

// =================系统主界面============

// 菜单按钮事件回调
static void sys_main_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
        lv_obj_t *grid = lv_obj_get_parent(btn);
        uint32_t total = lv_obj_get_child_cnt(grid);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t next_index = (index + 1) % total;
        lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }

    else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
        lv_obj_t *grid = lv_obj_get_parent(btn);
        uint32_t total = lv_obj_get_child_cnt(grid);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t next_index = (index + total - 1) % total;
        lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }
    
    else if (key == LV_KEY_ESC) {
        load_sys_settings_menu_screen(); // 返回上一级
        }

    else if (key == LV_KEY_ENTER) {
        if (strcmp(tag, "PARAM") == 0) {
            // 参数设置
            load_system_param_screen();
        }

        else if (strcmp(tag, "BASIC") == 0) {
            // 基础设置
            ui::system::load_sys_settings_basic_screen(); 
        }

        else if (strcmp(tag, "ADVANCED") == 0) {
            // 高级设置
            ui::system::load_system_advanced_screen();
        }

        else if (strcmp(tag, "Self-checking") == 0) { 
            load_self_check_menu_screen();

            // 延时退出
            lv_timer_t *t = lv_timer_create([](lv_timer_t*){ exit(0); }, 2000, nullptr);
            lv_timer_set_repeat_count(t, 1);
        }
     }
  }
}

// 主屏幕实现
void load_sys_settings_menu_screen() {
    if (scr_sys) lv_obj_delete(scr_sys);
    scr_sys = lv_obj_create(nullptr);
    lv_obj_add_style(scr_sys, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_SETTINGS, &scr_sys);

    lv_obj_t *title = lv_label_create(scr_sys);
    lv_label_set_text(title, "系统设置 / Settings");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Grid 布局
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50,50,50, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_t *grid = lv_obj_create(scr_sys);
    lv_obj_set_size(grid, 220, 260);
    lv_obj_center(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);

    //基础设置
    lv_obj_t *b1 = ::create_sys_grid_btn(grid, 0, LV_SYMBOL_SETTINGS, "Basic", "基础设置", sys_main_event_cb, "BASIC");

    // 高级设置
    lv_obj_t *b2 = ::create_sys_grid_btn(grid, 1, LV_SYMBOL_SETTINGS, "Advanced", "高级设置", sys_main_event_cb, "ADVANCED");

    // 参数设置
    lv_obj_t *b3 = ::create_sys_grid_btn(grid, 2, LV_SYMBOL_SETTINGS, "Params", "参数设置", sys_main_event_cb, "PARAM");

    // 自检功能
    lv_obj_t *b4 = ::create_sys_grid_btn(grid, 3, LV_SYMBOL_SETTINGS, "Self-checking", "自检功能", sys_main_event_cb, "Self-checking");

    UiManager::getInstance()->resetKeypadGroup();// 先清空组，后续重新添加以确保正确顺序
    UiManager::getInstance()->addObjToGroup(b1);//基础设置
    UiManager::getInstance()->addObjToGroup(b2);//高级设置
    UiManager::getInstance()->addObjToGroup(b3);//参数设置
    UiManager::getInstance()->addObjToGroup(b4);//自检功能  
    lv_group_focus_obj(b1);// 默认焦点在基础设置

    lv_obj_add_event_cb(scr_sys, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) ui::menu::load_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_sys);

    lv_screen_load(scr_sys);
    UiManager::getInstance()->destroyAllScreensExcept(scr_sys);
}

// 初始化系统设置
void init_system_settings() {
    load_config();
}

} // namespace system
} // namespace ui
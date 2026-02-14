#include "ui_widgets.h"
#include "ui_style.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>

#include "../../business/event_bus.h"// 用于时间更新订阅

// ====== 全局时间状态和回调函数 ======
static std::string g_latest_time_str = "00:00"; // 统一保存最新的时间字符串
static bool g_time_subscribed = false;          // 确保只订阅一次 EventBus

// 定时器回调：用于将最新时间同步给当前页面的 Label
static void time_sync_timer_cb(lv_timer_t * timer) {
    lv_obj_t * lbl = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (lbl) {
        // 如果最新时间和当前 Label 上的文字不一致，则更新 UI
        if (strcmp(lv_label_get_text(lbl), g_latest_time_str.c_str()) != 0) {
            lv_label_set_text(lbl, g_latest_time_str.c_str());
        }
    }
}

// 销毁回调：当 Label 被销毁时释放定时器，防止内存泄漏或野指针崩溃
static void time_label_del_cb(lv_event_t * e) {
    lv_timer_t * timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer) {
        lv_timer_delete(timer);
    }
}

// ================= 实现标准屏幕框架 (Header + Content + Footer) =================
BaseScreenParts create_base_screen(const char* title) {
    BaseScreenParts parts;

    // 1. 创建根屏幕 (应用蓝色背景)
    parts.screen = lv_obj_create(NULL);
    lv_obj_add_style(parts.screen, &style_base, 0);
    // 确保移除 padding，让内容贴边
    lv_obj_set_style_pad_all(parts.screen, 0, 0);


    // ================= 顶部状态栏 (Header) =================
    parts.header = lv_obj_create(parts.screen);
    lv_obj_set_size(parts.header, LV_PCT(100), UI_HEADER_H);
    lv_obj_align(parts.header, LV_ALIGN_TOP_MID, 0, 0);
    
    // 样式：玻璃质感 (渐变 + 边框)，并优化边框只显示在底部
    lv_obj_add_style(parts.header, &style_bar_glass, 0); // 玻璃质感样式 (包含渐变和边框)
    // 优化边框：顶部栏只需要"下边框"作为分割线，不需要四周都有边框
    lv_obj_set_style_border_side(parts.header, LV_BORDER_SIDE_BOTTOM, 0);

    // 稍微加点左右边距，不让文字贴着屏幕边缘
    lv_obj_set_style_pad_left(parts.header, 10, 0);
    lv_obj_set_style_pad_right(parts.header, 10, 0);

    // [Header 内容] 1. 页面标题 (左对齐)
    parts.lbl_title = lv_label_create(parts.header);
    lv_label_set_text(parts.lbl_title, title);
    lv_obj_add_style(parts.lbl_title, &style_text_cn, 0); // 中文字体
    lv_obj_set_style_text_color(parts.lbl_title, lv_color_white(), 0);// 白色字体
    lv_obj_align(parts.lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    // [Header 内容] 2. 时间/Wifi (右对齐)
    parts.lbl_time = lv_label_create(parts.header);
    lv_label_set_text(parts.lbl_time, g_latest_time_str.c_str()); // 初始显示最新时间
    lv_obj_add_style(parts.lbl_time, &style_text_cn, 0); // 中文字体
    lv_obj_set_style_text_color(parts.lbl_time, lv_color_white(), 0);
    lv_obj_align(parts.lbl_time, LV_ALIGN_RIGHT_MID, -10, 0);
    
    // ====== 时间自动化绑定逻辑 ======
    
    // (1) 保证全局仅订阅一次 EventBus
    if (!g_time_subscribed) {
        auto& bus = EventBus::getInstance();
        bus.subscribe(EventType::TIME_UPDATE, [](void* data) {
            std::string* t = static_cast<std::string*>(data);
            lv_async_call([](void* d){
                std::string* time_str = (std::string*)d;
                if (time_str && !time_str->empty()) {
                    g_latest_time_str = *time_str; // 更新全局最新时间
                }
                delete time_str; // 释放内存
            }, new std::string(*t));
        });
        g_time_subscribed = true;
    }
    // (2) 为当前创建的这个 lbl_time 分配一个刷新定时器 (每 500ms 检查一次)
    lv_timer_t * sync_timer = lv_timer_create(time_sync_timer_cb, 500, parts.lbl_time);
    // (3) 监听 lbl_time 的销毁事件，当离开或销毁该界面时，自动销毁定时器
    lv_obj_add_event_cb(parts.lbl_time, time_label_del_cb, LV_EVENT_DELETE, sync_timer);


    // ================= 底部状态栏 (Footer) =================
    parts.footer = lv_obj_create(parts.screen);
    lv_obj_set_size(parts.footer, LV_PCT(100), UI_FOOTER_H);
    lv_obj_align(parts.footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 样式：玻璃质感 (渐变 + 边框)，并优化边框只显示在顶部
    lv_obj_add_style(parts.footer, &style_bar_glass, 0);// 玻璃质感样式 (包含渐变和边框)
    // 优化边框：底部栏只需要"上边框"作为分割线
    lv_obj_set_style_border_side(parts.footer, LV_BORDER_SIDE_TOP, 0);

    // [Footer 内容1] 操作提示退出 (左对齐)
    lv_obj_t *lbl_out = lv_label_create(parts.footer);
    lv_label_set_text(lbl_out, "退出-ESC"); 
    lv_obj_set_style_text_color(lbl_out, lv_color_hex(0xDDDDDD), 0); // 稍微灰一点的白
    lv_obj_set_style_text_font(lbl_out, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_out, LV_ALIGN_LEFT_MID, 10, 0);

    // [Footer 内容2] 操作提示确认 (右对齐))
    lv_obj_t *lbl_notarize = lv_label_create(parts.footer);
    lv_label_set_text(lbl_notarize, "确认-OK"); 
    lv_obj_set_style_text_color(lbl_notarize, lv_color_hex(0xDDDDDD), 0); // 稍微灰一点的白
    lv_obj_set_style_text_font(lbl_notarize, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_notarize, LV_ALIGN_RIGHT_MID, -10, 0);


    // ================= 中间内容区 (Content) =================
    // 自动计算高度，填满 Header 和 Footer 之间的空间
    parts.content = lv_obj_create(parts.screen);
    lv_obj_set_size(parts.content, LV_PCT(100), SCREEN_H - UI_HEADER_H - UI_FOOTER_H);
    lv_obj_align(parts.content, LV_ALIGN_TOP_MID, 0, UI_HEADER_H); // 位于 Header 下方
    
    // 样式：透明，允许滚动
    lv_obj_set_style_bg_opa(parts.content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(parts.content, 0, 0);
    lv_obj_set_style_pad_all(parts.content, 0, 0);            // 去掉默认的内边距，让你的业务控件能贴边放
    lv_obj_set_scrollbar_mode(parts.content, LV_SCROLLBAR_MODE_OFF);// 默认不显示滚动条，内容过多时自动滚动
    return parts;
}

// 辅助函数：修改底部提示文字
void set_base_footer_hint(lv_obj_t* footer, const char* left_text, const char* right_text) {
    if (!footer) return;

    // 获取左侧 Label (第0个子对象)
    lv_obj_t* lbl_left = lv_obj_get_child(footer, 0); 
    if (lbl_left && left_text) {
        lv_label_set_text(lbl_left, left_text);
    }

    // 获取右侧 Label (第1个子对象)
    lv_obj_t* lbl_right = lv_obj_get_child(footer, 1);
    if (lbl_right && right_text) {
        lv_label_set_text(lbl_right, right_text);
    }
}

// ================= 实现标准系统菜单按钮 (Grid Item) =================
lv_obj_t* create_sys_grid_btn(
    lv_obj_t *parent, int row,
    const char* icon, const char* text_en, const char* text_cn,
    lv_event_cb_t event_cb, const char* user_data
) {
    lv_obj_t *btn = lv_button_create(parent);
    
    // 设置 Grid 位置 (占1列)
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, row, 1);

    // 应用样式
    lv_obj_add_style(btn, &style_btn_default, 0);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);

    // 布局: 横向排列
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 10, 0); 
    lv_obj_set_style_pad_gap(btn, 10, 0); 

    // 绑定事件
    // 注意：const char* user_data 需要确保生命周期，通常传字符串字面量是安全的
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, (void*)user_data);

    // 1. 图标
    lv_obj_t *lbl_icon = lv_label_create(btn);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_style_text_font(lbl_icon, g_font_icon_16, 0);

    // 2. 文字
    lv_obj_t *lbl_text = lv_label_create(btn);
    if (text_en && strlen(text_en) > 0) {
        lv_label_set_text_fmt(lbl_text, "%s  %s", text_en, text_cn);
    } else {
        lv_label_set_text(lbl_text, text_cn);
    }
    lv_obj_add_style(lbl_text, &style_text_cn, 0);

    return btn;
}

// ================= 实现居中面板创建函数 =================
lv_obj_t* create_center_panel(lv_obj_t* parent, int width) {
    // 1. 创建容器
    lv_obj_t * panel = lv_obj_create(parent);

    // 2. 核心布局设置
    lv_obj_set_width(panel, width);                 // 设置宽度
    lv_obj_set_height(panel, LV_SIZE_CONTENT);      // 高度随内容自动伸缩
    lv_obj_center(panel);                           // 在父容器中绝对居中
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF); // 【关键】永久隐藏滚动条

    // 3. 内部 Flex 布局 (垂直居中排列)
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 4. 间距设置
    lv_obj_set_style_pad_all(panel, 10, 0); // 内边距
    lv_obj_set_style_pad_gap(panel, 10, 0); // 控件间距

    // 5. 样式美化 (应用 ui_style.cpp 中的透明/半透明风格)
    // 假设 ui_style.h 中定义了 style_panel_transp，如果没有，这里直接手写样式也可以
    lv_obj_add_style(panel, &style_panel_transp, 0); 
    
    // 额外增强一点背景对比度，防止全透明看不清
    lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_20, 0); // 20% 黑色背景
    lv_obj_set_style_radius(panel, 12, 0);        // 圆角

    return panel;
}

lv_obj_t* create_menu_grid_container(lv_obj_t* parent) {
    // 1. 创建容器
    lv_obj_t* cont = lv_obj_create(parent);
    
    // 2. 核心布局：大小自适应 + 绝对居中
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(cont); // 【关键】让整个九宫格在屏幕正中间
    
    // 3. 去掉滚动条 (你提到的右边滑条)
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    // 4. 样式美化 (透明背景)
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0); // 容器本身无内边距

    // 5. 定义网格布局 (Grid)
    // 针对 240x320 屏幕，建议用 2列布局 (每列约 100px)
    static lv_coord_t col_dsc[] = {100, 100, LV_GRID_TEMPLATE_LAST}; 
    static lv_coord_t row_dsc[] = {90, 90, 90, LV_GRID_TEMPLATE_LAST}; // 3行
    
    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    
    // 6. 让网格内的单元格也居中对齐
    lv_obj_set_grid_align(cont, LV_GRID_ALIGN_CENTER, LV_GRID_ALIGN_CENTER);

    return cont;
}

// ================= 实现通用弹窗和关闭回调 =================
void mbox_close_event_cb(lv_event_t * e) {
    lv_obj_t * mbox = (lv_obj_t *)lv_event_get_user_data(e);
    if(mbox) lv_msgbox_close(mbox);
}

// 显示通用弹窗
void show_popup(const char* title, const char* msg) {
    lv_obj_t* mbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, msg);
    lv_obj_t* btn = lv_msgbox_add_footer_button(mbox, "Close");
    lv_obj_add_event_cb(btn, mbox_close_event_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_center(mbox);
}
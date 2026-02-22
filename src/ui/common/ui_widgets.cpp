#include "ui_widgets.h"
#include "ui_style.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>//用于 std::pair

#include "../../business/event_bus.h"// 用于时间更新订阅
#include "../../ui/ui_controller.h"// 用于获取当前时间字符串
#include "../../ui/managers/ui_manager.h"

// 静态变量：用于保存和恢复输入组，实现物理隔离
static lv_group_t * g_popup_group = nullptr;
static lv_group_t * g_prev_group = nullptr;

// ====== 全局时间状态和回调函数 ======
static std::string g_latest_time_str = "00:00"; // 统一保存最新的时间字符串
static std::string g_latest_weekday_str = "Mon";// 统一保存最新的星期字符串
static bool g_time_subscribed = false;          // 确保只订阅一次 EventBus

// 专门定义一个结构体，用来向异步任务传递上下文数据
struct PopupContext {
    lv_obj_t * overlay;
    lv_obj_t * focus_back_obj;
};

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

// 定时器回调：用于将最新星期同步给当前页面的 Label
static void weekday_sync_timer_cb(lv_timer_t * timer) {
    lv_obj_t * lbl = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (lbl) {
        if (strcmp(lv_label_get_text(lbl), g_latest_weekday_str.c_str()) != 0) {
            lv_label_set_text(lbl, g_latest_weekday_str.c_str());
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

//标准屏幕框架（顶部/底部渐变蓝，中间自动计算高度）
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

    // 设置 Header 布局模式
    // 使用 Row (行) 布局，子元素垂直居中
    lv_obj_set_flex_flow(parts.header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parts.header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // [Header 内容] 1.左侧：星期标签 (左对齐)
    lv_obj_t * lbl_weekday = lv_label_create(parts.header);
    lv_label_set_text(lbl_weekday, g_latest_weekday_str.c_str()); 
    lv_obj_set_style_text_color(lbl_weekday, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_weekday, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_weekday, 60); 
    lv_obj_set_style_text_align(lbl_weekday, LV_TEXT_ALIGN_LEFT, 0);
    lv_timer_t * timer_weekday = lv_timer_create(weekday_sync_timer_cb, 1000, lbl_weekday);// 每秒检查一次星期更新
    lv_obj_add_event_cb(lbl_weekday, time_label_del_cb, LV_EVENT_DELETE, timer_weekday);// 绑定销毁回调，自动删除定时器

    // [Header 内容] 2.中间: 页面标题 (居中)
    parts.lbl_title = lv_label_create(parts.header);
    lv_label_set_text(parts.lbl_title, title);
    lv_obj_add_style(parts.lbl_title, &style_text_cn, 0); // 中文字体
    lv_obj_set_style_text_color(parts.lbl_title, lv_color_white(), 0);// 白色字体
    lv_obj_set_flex_grow(parts.lbl_title, 1);
    lv_obj_set_style_text_align(parts.lbl_title, LV_TEXT_ALIGN_CENTER, 0);

    // [Header 内容] 2. 时间/Wifi (右对齐)
    parts.lbl_time = lv_label_create(parts.header);
    lv_label_set_text(parts.lbl_time, g_latest_time_str.c_str()); // 初始显示最新时间
    lv_obj_add_style(parts.lbl_time, &style_text_cn, 0); // 中文字体
    lv_obj_set_style_text_color(parts.lbl_time, lv_color_white(), 0);
    lv_obj_set_width(parts.lbl_time, 60); 
    lv_obj_set_style_text_align(parts.lbl_time, LV_TEXT_ALIGN_RIGHT, 0);
    
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
                    g_latest_weekday_str = UiController::getInstance()->getCurrentWeekdayStr(); // 同步更新全局最新星期
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


// ================= 实现标准按钮创建  =================

// 辅助函数：创建一个标准的系统菜单按钮，包含图标和文字，适用于列表布局(flex布局)
lv_obj_t* create_sys_list_btn(
    lv_obj_t *parent,
    const char* icon, const char* text_en, const char* text_cn,
    lv_event_cb_t event_cb, const char* user_data
) {
    lv_obj_t *btn = lv_button_create(parent);

    // 设置 Flex 位置 (占满整行，宽度100%)
    lv_obj_set_width(btn, LV_PCT(100)); // 宽度 100% 填满列表
    lv_obj_set_height(btn, LV_PCT(20)); // 高度占父容器的 20%，根据需要调整

    // 样式设置
    lv_obj_add_style(btn, &style_btn_default, 0);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);

    // 去掉圆角，变成直角矩形
    lv_obj_set_style_radius(btn, 0, 0); 

    // 覆盖默认全包围边框，只显示【底部】和【右侧】的 1px 分割线
    lv_obj_set_style_border_side(btn, static_cast<lv_border_side_t>(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT), 0);
    lv_obj_set_style_border_width(btn, 1, 0); 
    lv_obj_set_style_border_color(btn, lv_color_white(), 0); 
    lv_obj_set_style_border_opa(btn, LV_OPA_30, 0); 

    // 布局: 横向排列
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 10, 0); // 内边距
    lv_obj_set_style_pad_gap(btn, 10, 0); // 图标和文字之间的间距

    // 绑定事件
    if(event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, (void*)user_data);
    }

    // 预处理：判断有没有传值
    bool has_en = (text_en && strlen(text_en) > 0);
    bool has_cn = (text_cn && strlen(text_cn) > 0);

    // 1. 左侧图标 (只有当 icon 不为空时才创建)
    if (icon && strlen(icon) > 0) {
        lv_obj_t *lbl_icon = lv_label_create(btn);
        lv_label_set_text(lbl_icon, icon);
        lv_obj_add_style(lbl_icon, &style_text_cn, 0);
    }

    // 2. 左侧标题 (如 "员工管理")
    if (has_en) {
        lv_obj_t *lbl_title = lv_label_create(btn);
        lv_label_set_text(lbl_title, text_en);
        lv_obj_add_style(lbl_title, &style_text_cn, 0);
    }

    // 【智能透明弹簧占位块】
    // 它的唯一作用就是挤占中间的空白，把后面的数据推到最右侧
    //只有当 text_en 和 text_cn 都有值时，才把它们拆到左右两边！
    if (has_en && has_cn) {
        lv_obj_t * spacer = lv_obj_create(btn);
        lv_obj_set_height(spacer, 0); 
        lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0); 
        lv_obj_set_style_border_width(spacer, 0, 0);       
        lv_obj_set_style_pad_all(spacer, 0, 0);            
        lv_obj_set_flex_grow(spacer, 1);                   
    }

    // 4. 右侧数据 (如 "1001")
    if (has_cn) {
        lv_obj_t *lbl_value = lv_label_create(btn);
        lv_label_set_text(lbl_value, text_cn);
        lv_obj_add_style(lbl_value, &style_text_cn, 0);
    }

    return btn;
}

// 辅助函数：创建一个标准的系统菜单按钮，包含图标和文字，适用于九宫格布局(grid布局)
lv_obj_t* create_sys_grid_btn(
    lv_obj_t *parent, int row,
    const char* icon, const char* text_en, const char* text_cn,
    lv_event_cb_t event_cb, const char* user_data
) {
    lv_obj_t *btn = lv_button_create(parent);
    
    // 设置 Grid 位置 (占1列)
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, row, 1);// 占满整行，垂直拉伸

    // 样式设置
    lv_obj_add_style(btn, &style_btn_default, 0);// 默认样式
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);// 聚焦时样式
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);// 键盘聚焦时样式

    lv_obj_set_style_radius(btn, 0, 0); // 1. 去除圆角，变成直角矩形

    // 覆盖默认全包围边框，只显示【底部】和【右侧】的 1px 分割线
    lv_obj_set_style_border_side(btn, static_cast<lv_border_side_t>(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT), 0);
    lv_obj_set_style_border_width(btn, 1, 0); 
    lv_obj_set_style_border_color(btn, lv_color_white(), 0); 
    lv_obj_set_style_border_opa(btn, LV_OPA_30, 0); // 30% 透明的白线

    // 布局: 横向排列
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);// 横向排列
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);// 左对齐，垂直居中
    lv_obj_set_style_pad_all(btn, 10, 0); // 内边距
    lv_obj_set_style_pad_gap(btn, 10, 0); // 图标和文字之间的间距

    // 绑定事件
    // 注意：const char* user_data 需要确保生命周期，通常传字符串字面量是安全的
    if(event_cb){
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, (void*)user_data);
    }

    // 1. 图标 (只有当 icon 不为空时才创建)
    if (icon && strlen(icon) > 0) {
        lv_obj_t *lbl_icon = lv_label_create(btn);// 图标 Label
        lv_label_set_text(lbl_icon, icon);// 使用内置大字体图标
        lv_obj_set_style_text_font(lbl_icon, g_font_icon_16, 0);// 设置图标字体
    }

    // 2. 文字
    lv_obj_t *lbl_text = lv_label_create(btn);// 文字 Label
    if (text_en && strlen(text_en) > 0) {
        lv_label_set_text_fmt(lbl_text, "%s  %s", text_en, text_cn);
    } else {
        lv_label_set_text(lbl_text, text_cn);
    }
    lv_obj_add_style(lbl_text, &style_text_cn, 0);

    return btn;
}


// ================= 实现标准按钮容器创建函数 =================

// 辅助函数：创建一个适用于列表的 Flex 容器，预设为垂直排列，适合放置列表按钮(flex布局)
lv_obj_t* create_list_container(lv_obj_t* parent) {
    lv_obj_t * cont = lv_obj_create(parent);
    
    // 1. 尺寸占满父容器可用空间
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    
    // 2. 自动滚动条模式！
    // 效果：内容高度 <= 容器高度时，无滚动条、不可滚动；
    //      内容高度 > 容器高度时，自动出现滚动条，可丝滑滚动。
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    
    // 3. 样式美化：透明背景，无边框，四周留出一点点内边距
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 5, 0); 
    
    // 4. 使用 Flex 垂直弹性布局
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN); // 从上往下排成一列
    
    // START 表示从顶部开始贴紧排列，CENTER 表示水平居中
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 5. 设置每行按钮之间的垂直间距 (例如 5 像素)
    lv_obj_set_style_pad_row(cont, 0, 0); 

    return cont;
}

// 辅助函数：创建一个适用于菜单的 Grid 容器，预设为 2列布局，适合放置九宫格按钮(grid布局)
lv_obj_t* create_menu_grid_container(lv_obj_t* parent) {
    // 1. 创建容器
    lv_obj_t* cont = lv_obj_create(parent);
    
    // 2. 核心布局：大小自适应 + 绝对居中
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);// 大小随内容自动适应
    lv_obj_center(cont); // 让整个九宫格在屏幕正中间
    
    // 3. 去掉滚动条
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    // 4. 样式美化 (透明背景)
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0); // 容器本身无内边距
    lv_obj_set_style_pad_row(cont, 0, 0); // 行间距由子对象控制，保持紧凑
    lv_obj_set_style_pad_column(cont, 0, 0);// 列间距由子对象控制，保持紧凑

    // 5. 定义网格布局 (Grid)
    // 针对 240x320 屏幕，建议用 2列布局 (每列约 100px)
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // 2列，每列100px，最后一个必须是 LV_GRID_TEMPLATE_LAST
    static lv_coord_t row_dsc[] = {90, 90, 90, LV_GRID_TEMPLATE_LAST}; // 3行，每行90px，最后一个必须是 LV_GRID_TEMPLATE_LAST
    
    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);// 设置 Grid 行列描述
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);// 设置布局为 Grid
    
    // 6. 让网格内的单元格也居中对齐
    lv_obj_set_grid_align(cont, LV_GRID_ALIGN_CENTER, LV_GRID_ALIGN_CENTER);

    return cont;
}


// ================= 实现标准辅助输入框创建函数 =================

// 辅助函数：创建一个带标签的输入框，适用于表单输入场景
lv_obj_t* create_form_input(
    lv_obj_t *parent, 
    const char *label_text, 
    const char *placeholder_text, 
    const char *initial_text, 
    bool is_readonly
) {
    // ==========================================
    // 1. 创建一个透明的横向包裹容器 (Wrapper)
    // ==========================================
    lv_obj_t * wrapper = lv_obj_create(parent);
    lv_obj_set_width(wrapper, LV_PCT(100));       // 占据父容器 100% 的宽度
    lv_obj_set_height(wrapper, LV_PCT(20)); // 高度由内部控件撑开

    // 清除默认背景、边框和内边距，让它完全隐形
    lv_obj_set_style_bg_opa(wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrapper, 0, 0);
    lv_obj_set_style_pad_all(wrapper, 0, 0);

    // 设置横向 Flex 布局：左对齐，垂直居中对齐
    lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wrapper, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(wrapper, 5, 0);    // 标签和输入框之间的间距为 5px

    // ==========================================
    // 2. 创建左侧标题 Label (放进 wrapper 里)
    // ==========================================
    lv_obj_t* lbl = lv_label_create(wrapper);
    lv_label_set_text(lbl, label_text);
    lv_obj_add_style(lbl, &style_text_cn, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0); 
    lv_obj_set_width(lbl, LV_PCT(32)); // 设置标签宽度为容器宽度的30%
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_RIGHT, 0);

    // ==========================================
    // 3. 创建右侧输入框 TextArea (放进 wrapper 里)
    // ==========================================
    lv_obj_t* ta = lv_textarea_create(wrapper);
    lv_textarea_set_one_line(ta, true);
    lv_obj_add_style(ta, &style_text_cn, 0);
    lv_obj_set_flex_grow(ta, 1); // 让输入框自动伸缩，占满同行剩下的所有空间！
    lv_obj_set_style_radius(ta, 0, 0); // 去除圆角，变成直角矩形

    // ==========================================
    // 4. 填充数据与状态处理
    // ==========================================
    if (placeholder_text != nullptr && strlen(placeholder_text) > 0) {
        lv_textarea_set_placeholder_text(ta, placeholder_text);
    }
    if (initial_text != nullptr && strlen(initial_text) > 0) {
        lv_textarea_set_text(ta, initial_text);
    }

    if (is_readonly) {
        lv_obj_remove_flag(ta, LV_OBJ_FLAG_CLICKABLE);                // 不可点击聚焦
        lv_obj_set_style_bg_color(ta, lv_color_hex(0x555555), 0);     // 背景变暗灰提示不可写
    }

    // 返回输入框指针
    return ta; 
}

// 辅助函数：创建一个表单下拉框，适用于下拉选择的场景
lv_obj_t* create_form_dropdown(lv_obj_t* parent, const char* title, const std::vector<std::pair<int, std::string>>& items, int default_id) {
    // 1. 创建 UI 容器和布局
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, lv_pct(20));

    // 清除默认背景、边框和内边距，让它完全隐形
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);

    // 设置横向 Flex 布局：左对齐，垂直居中对齐
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row, 5, 0);    // 标签和下拉框之间的间距为 5px

    // ==========================================
    // 2. 创建左侧标题 Label (放进 row 里)
    // ==========================================
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, title);
    lv_obj_add_style(label, &style_text_cn, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0); 
    lv_obj_set_width(label, LV_PCT(30)); // 设置标签宽度为容器宽度的30%
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);

    // ==========================================
    // 3. 创建右侧下拉框 Dd (放进 row 里)
    // ==========================================
    lv_obj_t* dd = lv_dropdown_create(row);
    lv_textarea_set_one_line(dd, true);
    lv_obj_add_style(dd, &style_text_cn, 0);
    lv_obj_set_flex_grow(dd, 1); // 让输入框自动伸缩，占满同行剩下的所有空间！
    lv_obj_set_style_radius(dd, 0, 0); // 去除圆角，变成直角矩形

    // 4. 解析传入的通用数据 items
    std::string options_str = "";
    int default_sel_index = 0;

    for (size_t i = 0; i < items.size(); ++i) {
        options_str += items[i].second; // 取出名称拼接到字符串中
        if (i < items.size() - 1) {
            options_str += "\n";
        }
        // 如果传入的 default_id 等于当前项的 ID，记录其索引
        if (items[i].first == default_id) {
            default_sel_index = i;
        }
    }
    
    // 5. 应用数据并设置默认选中项
    lv_dropdown_set_options(dd, options_str.c_str());
    lv_dropdown_set_selected(dd, default_sel_index);

    return dd; 
}


// ========================== 实现适用于表单输入框的标准容器创建函数 ==================

// 辅助函数：创建一个适用于表单输入框的标准容器，预设为垂直排列，适合放置多个输入框(flex布局)
lv_obj_t* create_form_container(lv_obj_t* parent) {
    lv_obj_t* form_cont = lv_obj_create(parent);
    
    // 尺寸占满可用区域
    lv_obj_set_size(form_cont, LV_PCT(100), LV_PCT(100));
    
    // 如果表单项太多超出屏幕，自动开启滚动条
    lv_obj_set_scrollbar_mode(form_cont, LV_SCROLLBAR_MODE_AUTO);
    
    // 设置垂直 Flex 布局
    lv_obj_set_flex_flow(form_cont, LV_FLEX_FLOW_COLUMN);
    // 从上到下排列，水平居中对齐，垂直居中对齐
    lv_obj_set_flex_align(form_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 统一的表单样式：透明背景，无边框
    lv_obj_set_style_bg_opa(form_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form_cont, 0, 0);
    
    // 统一的间距控制 (修改这里，全系统的表单间距都会跟着变)
    lv_obj_set_style_pad_all(form_cont, 10, 0); // 容器四周的留白
    lv_obj_set_style_pad_gap(form_cont, 15, 0); // 每个表单输入框之间的上下间距

    return form_cont;
}


// ========================== 实现通用确认按钮创建函数  ==========================

// 辅助函数：创建一个确认按钮，适用于表单提交，支持事件绑定和用户数据传递
lv_obj_t* create_form_btn(lv_obj_t *parent, const char *btn_text, lv_event_cb_t event_cb, void *user_data) {
    // 1. 创建按钮本体
    lv_obj_t* btn = lv_button_create(parent);
    
    // 宽度设为 90%，和我们刚才写的 create_form_input 保持完美的视觉对齐
    lv_obj_set_width(btn, LV_PCT(90)); 
    
    // 2. 添加默认样式和聚焦样式
    lv_obj_add_style(btn, &style_btn_default, 0);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(btn, 0, 0); // 去除圆角，变成直角矩形

    // 3. 绑定事件 (如果有传的话)
    if (event_cb != nullptr) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, user_data);
    }

    // 4. 创建按钮上的居中文字
    lv_obj_t* lbl = lv_label_create(btn);
    if (btn_text != nullptr) {
        lv_label_set_text(lbl, btn_text);
    }
    lv_obj_add_style(lbl, &style_text_cn, 0);
    lv_obj_center(lbl); // 让文字在按钮正中间

    return btn;
}


// ================= 实现通用弹窗和关闭回调 =================

// 异步销毁任务 
static void popup_close_async_task(void * p) {
    PopupContext * ctx = (PopupContext *)p;

    // 1. 将键盘控制权安全地还给背景界面
    lv_indev_t * indev = lv_indev_get_next(nullptr);
    while(indev) {
        if(lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            if (g_prev_group) {
                lv_indev_set_group(indev, g_prev_group);
            }
            break;
        }
        indev = lv_indev_get_next(indev);
    }

    // 2. 销毁弹窗专属的临时组
    if (g_popup_group) {
        lv_group_delete(g_popup_group);
        g_popup_group = nullptr;
    }

    // 3. 恢复焦点到原界面的报错控件上
    if (ctx->focus_back_obj) {
        lv_group_focus_obj(ctx->focus_back_obj);
    }

    // 4. 销毁遮罩层（连同内部的弹窗主体一起）
    if (ctx->overlay) {
        lv_obj_delete(ctx->overlay);
    }

    delete ctx; // 释放结构体内存，防止内存泄漏
}

//  弹窗销毁的回调函数 
static void popup_close_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = (code == LV_EVENT_KEY) ? lv_event_get_key(e) : 0;

    // 仅当点击，或者按下 回车(ENTER) / 返回(ESC) 时才触发关闭
    if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_ESC))) {
        
        lv_obj_t * target = static_cast<lv_obj_t *>(lv_event_get_target(e));
        
        // 防抖：立刻移除事件回调，防止按键由于机械抖动或长按触发多次崩溃
        lv_obj_remove_event_cb(target, popup_close_event_cb);
        
        PopupContext * ctx = (PopupContext *)lv_event_get_user_data(e);

        // 使用异步调用，等 LVGL 底层把当前的键盘事件处理透彻后，再执行销毁逻辑
        lv_async_call(popup_close_async_task, ctx);
    }
}

/**
 * @brief 显示一个通用单按钮弹窗
 * @param title 弹窗标题 (传 nullptr 则不显示标题)
 * @param msg   弹窗内容
 * @param focus_back_obj 弹窗关闭后，焦点需要回到哪个控件上 (防止焦点丢失)
 */
void show_popup_msg(const char* title, const char* msg, lv_obj_t* focus_back_obj, const char* btn_text) {
    
    // 【防重入保护】：如果当前已经有弹窗了，直接忽略，防止界面组嵌套死锁
    if (g_popup_group != nullptr) {
        return; 
    }

    // 1. 创建全屏遮罩层
    lv_obj_t * overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0); 
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 创建弹窗主体容器
    lv_obj_t * popup = lv_obj_create(overlay);
    lv_obj_set_width(popup, LV_PCT(80)); 
    lv_obj_set_height(popup, LV_SIZE_CONTENT);
    lv_obj_align(popup, LV_ALIGN_CENTER, 0, 0); 
    
    // 去掉圆角，去掉全局内边距（为了让标题栏贴边）
    lv_obj_set_style_pad_all(popup, 0, 0);
    lv_obj_set_style_radius(popup, 0, 0); 
    lv_obj_set_style_bg_color(popup, lv_color_white(), 0);
    // 加上硬朗的边框，更符合你参考图的风格
    lv_obj_set_style_border_width(popup, 1, 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x007AFF), 0); // 使用蓝色边框

    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_set_layout(popup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(popup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 3. 渲染标题 (独立成一个带背景的栏目)
    if (title && strlen(title) > 0) {
        // 创建蓝色标题栏容器
        lv_obj_t * title_bar = lv_obj_create(popup);
        lv_obj_set_width(title_bar, LV_PCT(100));     // 宽度占满主容器
        lv_obj_set_height(title_bar, LV_SIZE_CONTENT); // 高度由内部文字撑开
        lv_obj_set_style_radius(title_bar, 0, 0);     // 无圆角
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x0055FF), 0); // 匹配你图上的经典蓝
        lv_obj_set_style_border_width(title_bar, 0, 0); // 无边框
        lv_obj_set_style_pad_all(title_bar, 8, 0);    // 标题栏内边距
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        // 在标题栏内创建文字
        lv_obj_t * lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, title);
        lv_obj_add_style(lbl_title, &style_text_cn, 0); // 添加中文字体防止乱码
        lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0); // 标题文字设为白色
        lv_obj_center(lbl_title); // 居中显示
    }

    // 4. 渲染消息文本
    lv_obj_t * lbl_msg = lv_label_create(popup);
    lv_label_set_text(lbl_msg, msg);
    lv_obj_add_style(lbl_msg, &style_text_cn, 0); // 添加中文字体
    lv_label_set_long_mode(lbl_msg, LV_LABEL_LONG_WRAP); 
    lv_obj_set_width(lbl_msg, LV_PCT(90)); 
    lv_obj_set_style_text_align(lbl_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(0x333333), 0);
    
    // 因为主容器去掉了 padding，这里要专门给文字上下加间距
    lv_obj_set_style_pad_top(lbl_msg, 25, 0); 
    lv_obj_set_style_pad_bottom(lbl_msg, 20, 0);

    // 5. 渲染“确认”按钮
    lv_obj_t * btn_ok = lv_btn_create(popup);
    lv_obj_set_width(btn_ok, LV_PCT(60));
    lv_obj_set_height(btn_ok, 35);
    lv_obj_set_style_radius(btn_ok, 0, 0); // 按钮也顺应风格去掉圆角
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x007AFF), 0); 
    lv_obj_set_style_margin_bottom(btn_ok, 20, 0); // 底部留出空白

    // 明确设置按钮的“聚焦态”样式
    lv_obj_add_style(btn_ok, &style_btn_default, 0);// 默认样式
    lv_obj_add_style(btn_ok, &style_btn_focused, LV_STATE_FOCUSED);// 聚焦时样式
    lv_obj_add_style(btn_ok, &style_btn_focused, LV_STATE_FOCUS_KEY);// 键盘聚焦时样式

    lv_obj_t * lbl_btn = lv_label_create(btn_ok);
    lv_label_set_text(lbl_btn, (btn_text && strlen(btn_text) > 0) ? btn_text : "确认");// 增加一个判空保护，如果传了空指针依然显示"确认"
    lv_obj_add_style(lbl_btn, &style_text_cn, 0); // 添加中文字体
    lv_obj_center(lbl_btn);

    // 6. 绑定事件与上下文
    PopupContext * ctx = new PopupContext;
    ctx->overlay = overlay;
    ctx->focus_back_obj = focus_back_obj;

    lv_obj_add_event_cb(btn_ok, popup_close_event_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_ok, popup_close_event_cb, LV_EVENT_KEY, ctx);

    // 7. 物理隔离与抢占焦点
    lv_indev_t * indev = lv_indev_get_next(nullptr);
    while(indev) {
        if(lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            
            g_prev_group = lv_indev_get_group(indev); 
            
            g_popup_group = lv_group_create();        
            lv_group_add_obj(g_popup_group, btn_ok);  
            
            lv_indev_set_group(indev, g_popup_group); 
            lv_group_focus_obj(btn_ok);               
            break;
        }
        indev = lv_indev_get_next(indev);
    }
}

// 弹窗关闭事件回调
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

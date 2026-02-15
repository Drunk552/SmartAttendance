#include "ui_scr_menu.h"
#include <cstring>
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"

// 引入各模块入口
#include "../home/ui_scr_home.h"
#include "../user_mgmt/ui_user_mgmt.h"
#include "../sys_info/ui_scr_sys_info.h"
#include "../att_stats/ui_scr_att_stats.h" 
#include "../att_design/ui_scr_att_design.h"
#include "../record_query/ui_scr_record_query.h"
#include "../system/ui_sys_settings.h"

namespace ui {
namespace menu {

static lv_obj_t *screen_menu = nullptr;
static lv_obj_t *obj_grid = nullptr;
static bool g_disk_full = false;            // 磁盘满标志

// 菜单项结构体 (保持与您原代码一致)
struct MenuEntry {
    const char* icon;       // 图标
    const char* text_en;    // 英文标题
    const char* text_cn;    // 中文标题
    const char* event_tag;  // 事件回调用的 Tag
};

// 主菜单按钮事件回调
static void menu_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    // 使用 C 风格强转：
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = static_cast<const char*>(lv_event_get_user_data(e));

    uint32_t key = 0;
    // 获取按键值 (如果不是按键事件，key 会是 0)
    if(code==LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 仅处理按键事件
    if (code == LV_EVENT_KEY) {

        if(key == LV_KEY_ESC) {
            ui::home::load_screen(); // 按 ESC 返回主页
            return; // 处理完返回后直接返回，避免继续执行下面的导航逻辑
        }
        
        // 获取当前按钮在 Grid 中的索引 (0, 1, 2, 3)
        // 0:左上, 1:右上, 2:左下, 3:右下
        lv_obj_t *grid = lv_obj_get_parent(btn);
        uint32_t index = lv_obj_get_index(btn); 
        uint32_t total = lv_obj_get_child_cnt(grid);
        int next_index = -1; // 目标索引

        // --- 核心导航逻辑 ---
        if (key == LV_KEY_RIGHT) {
            // 向右：+1，循环
            next_index = (index + 1) % total;
            std::printf("[UI] Nav: RIGHT (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_LEFT) {
            // 向左：-1，循环 (加 total 防止负数)
            next_index = (index + total - 1) % total;
            std::printf("[UI] Nav: LEFT (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_DOWN) {
            // 向下：+2 (因为是2列布局)，循环
            next_index = (index + 2) % total;
            std::printf("[UI] Nav: DOWN (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_UP) {
            // 向上：-2，循环
            next_index = (index + total - 2) % total;
            std::printf("[UI] Nav: UP (%d -> %d)\n", index, next_index);
        }

        // --- 执行跳转 ---
        if (next_index >= 0 && next_index < (int)total) {
            // 找到目标按钮
            lv_obj_t *target_btn = lv_obj_get_child(grid, next_index);
            // 强制聚焦
            lv_group_focus_obj(target_btn);
            return; // 完成跳转，直接返回
        }
    }
    
    // ==========================================
    // 2. 触发跳转逻辑 (CLICKED)
    // ==========================================
    // 无论是 触摸屏点击 还是 键盘按回车，最终都会触发这个事件
    if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        
        lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---

        if (tag == nullptr) return;// 安全检查

        // 根据 Tag 跳转到对应模块
        if (strcmp(tag, "UserMgmt") == 0) {
            ui::user_mgmt::load_user_menu_screen(); // 1.进入员工管理页面
        }
        else if (strcmp(tag, "Records") == 0) {
            ui::record_query::load_record_query_menu_screen();// 2.进入记录查询页面
        }
        else if (strcmp(tag, "STATS") == 0) {
            ui::att_stats::load_att_stats_menu_screen();// 3.进入考勤统计页面
        }
        else if (strcmp(tag, "AttDesign") == 0) {
            ui::att_design::load_att_design_menu_screen();// 4.进入考勤设计页
        }
        else if (strcmp(tag, "System") == 0) {
            ui::system::load_sys_settings_menu_screen();// 5.进入系统设置页面
        }
        else if (strcmp(tag, "SysInfo") == 0) {
            ui::sys_info::load_sys_info_menu_screen();// 6.进入系统信息页面
        }
    }
}

// 主屏幕实现
void load_menu_screen() {
    // 1. 创建屏幕
    if (screen_menu){
        lv_obj_delete(screen_menu);
        screen_menu = nullptr;
    }

    BaseScreenParts parts = create_base_screen("主菜单");
    screen_menu = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::MENU, &screen_menu);

    // 绑定销毁回调
    lv_obj_add_event_cb(screen_menu, [](lv_event_t * e) {
        screen_menu = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 4. 定义九宫格布局
    //使用 LV_GRID_FR(1) 让两列平分宽度，自动填满容器
    static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; 
    //行高也可以设为 FR 让其平分高度，或者保持固定值
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    obj_grid = lv_obj_create(parts.content); 
    lv_obj_set_size(obj_grid, 220, LV_PCT(96)); // 宽度适应屏幕，高度自适应
    lv_obj_center(obj_grid); // 在内容区居中
    lv_obj_set_layout(obj_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_grid, col_dsc, row_dsc);
    
    // 添加行列间距 (Gap)
    lv_obj_set_style_pad_row(obj_grid, 10, 0);    // 行间距 10px
    lv_obj_set_style_pad_column(obj_grid, 10, 0); // 列间距 10px

    // 样式：透明背景，无边框
    lv_obj_set_style_bg_opa(obj_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_grid, 0, 0);
    lv_obj_set_style_pad_all(obj_grid, 0, 0); // 清除内边距

    // 5. 菜单内容定义 
    static MenuEntry menu_items[] = {
        {LV_SYMBOL_DIRECTORY, "User Mgmt", "员工管理", "UserMgmt"}, 
        {LV_SYMBOL_EYE_OPEN,  "Records",   "记录查询", "Records"},
        {LV_SYMBOL_DRIVE,     "Att. Stats","考勤统计", "STATS"},
        {LV_SYMBOL_SETTINGS,  "System",    "系统设置", "System"},
        {LV_SYMBOL_LIST,      "Sys Info",      "系统信息", "SysInfo"},
        {LV_SYMBOL_EDIT,      "Att. Design",    "考勤设计", "AttDesign"},
    };
    
    // 6. 循环创建按钮
    for(int i = 0; i < 6; i++) {
        uint8_t col = i % 2;
        uint8_t row = i / 2;

        // 创建按钮主体
        lv_obj_t *btn = lv_button_create(obj_grid);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                             LV_GRID_ALIGN_STRETCH, row, 1);
        
        lv_obj_add_style(btn, &style_btn_default, 0); 
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUS_KEY);

        // 上下图文布局
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(btn, 5, 0); // 稍微增加间距美观

        // 添加事件
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_ALL, (void*)(menu_items[i].event_tag));

        // 6.1 图标
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, menu_items[i].icon);
        // 使用内置大字体图标
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);

        // 6.2 文字 (英文 + 中文)
        lv_obj_t *lbl = lv_label_create(btn);
        // 换行显示
        lv_label_set_text_fmt(lbl, "%s\n%s", menu_items[i].text_en, menu_items[i].text_cn);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        
        // 使用全局样式 style_text_cn
        lv_obj_add_style(lbl, &style_text_cn, 0);
        
        UiManager::getInstance()->addObjToGroup(btn);
    }

    // 默认聚焦第一个
    if(lv_obj_get_child_cnt(obj_grid) > 0)
        lv_group_focus_obj(lv_obj_get_child(obj_grid, 0));

    // 兜底 ESC
    lv_obj_add_event_cb(screen_menu, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) ui::home::load_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(screen_menu);

    lv_screen_load(screen_menu);
    UiManager::getInstance()->destroyAllScreensExcept(screen_menu);
}

} // namespace menu
} // namespace ui
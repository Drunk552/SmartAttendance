#include "ui_scr_att_design.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../menu/ui_scr_menu.h"
#include "../../ui_controller.h"
#include <cstring> 

namespace ui {
namespace att_design {

// ================= [内部状态: 屏幕指针] =================
static lv_obj_t *scr_design = nullptr;   // 考勤设计主菜单屏幕
static lv_obj_t *scr_dept = nullptr;     // 部门设置屏幕
static lv_obj_t *scr_shift_set = nullptr;    // 班次设置屏幕
static lv_obj_t *scr_rule = nullptr;     // 考勤规则屏幕
static lv_obj_t *scr_schedule = nullptr; // 人员排班屏幕
static lv_obj_t *scr_company = nullptr; // 公司设置屏幕
static lv_obj_t *scr_bell = nullptr;     // 定时响铃屏幕 
static lv_obj_t *scr_shift_info = nullptr; //班次详细信息界面

// ================= [内部状态: 输入框指针] =================
static lv_obj_t *ta_company_save = nullptr;    // 公司名称输入框 
static lv_obj_t* ta_dept_name = nullptr;// 全局变量用于存储详情界面的输入框
static std::array<lv_obj_t*, 14> ta_schedule_inputs; // 7 天×2 个输入框
static lv_obj_t *g_ta_time_frame1 = nullptr;//班次信息时段一输入框
static lv_obj_t *g_ta_time_frame2 = nullptr;//班次信息时段二输入框
static lv_obj_t *g_ta_overtime = nullptr;//班次信息加班时段输入框

// ================= [内部状态: 控件与数据] =================
static lv_obj_t *btn_company_save = nullptr;   // 保存按钮 
static lv_obj_t* dept_list_container = nullptr;  // 部门列表容器
static lv_obj_t *g_btn_a1_amend = nullptr;//班次信息界面修改按钮

// ================= [内部状态: 数据暂存] =================
static int g_selected_dept_id = -1;              // 当前选中的部门 ID
static int g_current_edit_shift_id = -1; // 记录当前选择的班次ID

// ====================== [辅助函数] ======================

// 文本框值改变时的回调：实现输入自动补全 ':' 和 '-'
static void time_input_format_cb(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    const char *txt = lv_textarea_get_text(ta);
    size_t len = strlen(txt);

    // 只有在用户输入长度刚好是 2, 5, 8 且末尾是数字时，才自动补全符号
    // 这样设计可以避免用户按退格键删除符号时陷入死循环
    if (len > 0) {
        char last_char = txt[len - 1];
        if (last_char >= '0' && last_char <= '9') {
            if (len == 2) {
                lv_textarea_add_text(ta, ":");
            } else if (len == 5) {
                lv_textarea_add_text(ta, "-");
            } else if (len == 8) {
                lv_textarea_add_text(ta, ":");
            }
        }
    }
}

// 校验字符串是否满足 HH:MM-HH:MM，并限制具体时间范围
static bool validate_and_parse_time(const std::string& input, std::string& start, std::string& end) {
    if (input.empty()) { // 为空是合法的，表示不排该时段
        start = "";
        end = "";
        return true;
    }
    
    // 长度必须刚好是 11
    if (input.length() != 11 || input[2] != ':' || input[5] != '-' || input[8] != ':') {
        return false;
    }

    start = input.substr(0, 5);
    end = input.substr(6, 5);

    // 解析数字，严格校验 小时 0-23，分钟 0-59 (若业务要求是 0-12 可以将 23 替换为 12)
    int h1 = std::stoi(start.substr(0, 2));
    int m1 = std::stoi(start.substr(3, 2));
    int h2 = std::stoi(end.substr(0, 2));
    int m2 = std::stoi(end.substr(3, 2));

    if (h1 < 0 || h1 > 23 || m1 < 0 || m1 > 59 ||
        h2 < 0 || h2 > 23 || m2 < 0 || m2 > 59) {
        return false;
    }

    return true;
}

// ================== [时间合法性校验辅助函数] ==================

// 1. 校验单个时间字符串的格式是否合法 (00:00 - 23:59)
static bool is_valid_time_format(const std::string& time_str) {
    // 允许为空，代表该时段不设置排班
    if (time_str.empty()) return true; 

    // 长度必须是 11 (HH:MM-HH:MM)
    if (time_str.length() != 11) return false;
    // 中间必须是冒号和横杠
    if (time_str[2] != ':' || time_str[5] != '-' || time_str[8] != ':') return false;

    // 校验前一半 (上班时间)
    int h1 = std::stoi(time_str.substr(0, 2));
    int m1 = std::stoi(time_str.substr(3, 2));
    // 校验后一半 (下班时间)
    int h2 = std::stoi(time_str.substr(6, 2));
    int m2 = std::stoi(time_str.substr(9, 2));

    return (h1 >= 0 && h1 <= 23) && (m1 >= 0 && m1 <= 59) &&
           (h2 >= 0 && h2 <= 23) && (m2 >= 0 && m2 <= 59);
}

// 2. 校验时间段内部逻辑（上班时间必须 < 下班时间）
static bool is_valid_time_range(const std::string& time_str) {
    if (time_str.empty()) return true; 

    int start_hh = std::stoi(time_str.substr(0, 2));
    int start_mm = std::stoi(time_str.substr(3, 2));
    int end_hh   = std::stoi(time_str.substr(6, 2));
    int end_mm   = std::stoi(time_str.substr(9, 2));

    int start_total_mins = start_hh * 60 + start_mm;
    int end_total_mins   = end_hh * 60 + end_mm;

    // 上班时间必须严格小于下班时间
    return start_total_mins < end_total_mins; 
}

// ==================== 主菜单事件回调 ====================
// 菜单按钮事件回调
static void design_event_cb(lv_event_t *e) {
    const char* tag = (const char*)lv_event_get_user_data(e); // 获取按钮的 tag
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
        }
        else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
        }
        // 左右导航（如果是多列布局，也可以支持）
        else if (key == LV_KEY_RIGHT) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
        }
        else if (key == LV_KEY_LEFT) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
        }
        // ESC 返回主菜单
        else if (key == LV_KEY_ESC) {
            ui::menu::load_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            if (strcmp(tag, "DEPT") == 0) {
                load_dept_screen(); // 跳转到部门设置界面
            } else if (strcmp(tag, "SHIFT") == 0) {
                load_shift_set_screen(); // 跳转到班次设置界面
            } else if (strcmp(tag, "RULE") == 0) {
                load_rule_screen(); // 跳转到考勤规则界面
            } else if (strcmp(tag, "SCH") == 0) {
                load_schedule_screen(); // 跳转到排班设置界面
            } else if (strcmp(tag, "COMPANY") == 0) {
                load_company_screen(); // 跳转到公司设置界面
            } else if (strcmp(tag, "BELL") == 0) {
                load_bell_screen(); // 跳转到定时响铃界面 
            }
        }
    }
}

// 加载考勤设计菜单实现
void load_att_design_menu_screen() {
    // 先删除旧屏幕（如果存在），避免重复创建
    if (scr_design){
        lv_obj_delete(scr_design);
        scr_design = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("考勤设计");
    scr_design = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::ATT_DESIGN, &scr_design);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_design, [](lv_event_t * e) {
        scr_design = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup(); // 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content); // 创建统一列表容器

    create_sys_list_btn(list, "1. ", "", "部门设置", design_event_cb, "DEPT");
    create_sys_list_btn(list, "2. ", "", "班次设置", design_event_cb, "SHIFT");
    create_sys_list_btn(list, "3. ", "", "考勤规则", design_event_cb, "RULE");
    create_sys_list_btn(list, "4. ", "", "排班设置", design_event_cb, "SCH");
    create_sys_list_btn(list, "5. ", "", "公司设置", design_event_cb, "COMPANY");
    create_sys_list_btn(list, "6. ", "", "定时响铃", design_event_cb, "BELL");
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_design, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) {
            ui::menu::load_menu_screen(); 
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_design);

    // 加载与清理
    lv_screen_load(scr_design);
    UiManager::getInstance()->destroyAllScreensExcept(scr_design);
}

// ==================== 部门设置子界面 ====================



// 刷新部门列表
static void refresh_dept_list() {
    if (dept_list_container) {
        lv_obj_clean(dept_list_container);
    }

    std::vector<DeptInfo> depts = UiController::getInstance()->getDepartmentList();

    for (const auto& dept : depts) {
        int emp_count = UiController::getInstance()->getDepartmentEmployeeCount(dept.id);

        // 创建部门项按钮
        lv_obj_t* item = lv_btn_create(dept_list_container);
        lv_obj_set_size(item, lv_pct(100), 50);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // 存储部门 ID 到 user_data
        int* dept_id_ptr = new int(dept.id);
        lv_obj_set_user_data(item, dept_id_ptr);

// 销毁时释放内存 ✅
        lv_obj_add_event_cb(item, +[](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);  // ✅ 先转换类型
        int* dept_id = (int*)lv_obj_get_user_data(target);
        if (dept_id) 
        {
            delete dept_id;
        }
    }
}, LV_EVENT_ALL, nullptr);

        // 点击事件
        lv_obj_add_event_cb(item, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            
            lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
            int* dept_id = (int*)lv_obj_get_user_data(target);
            if (dept_id) {
                g_selected_dept_id = *dept_id;
                
                // 清除所有项的选中状态 ✅
                lv_obj_t* container = lv_obj_get_parent(target);
                uint32_t cnt = lv_obj_get_child_cnt(container);
                for (uint32_t i = 0; i < cnt; i++) {
                    lv_obj_t* child = lv_obj_get_child(container, i);
                    lv_obj_remove_state(child, LV_STATE_CHECKED);
                }
                
                // 高亮当前选中的
                lv_obj_add_state(target, LV_STATE_CHECKED);
            }
        }, LV_EVENT_ALL, nullptr);

        // 部门名称
        lv_obj_t* name_label = lv_label_create(item);
        std::string name_text = "部门" + std::to_string(dept.id) + ": " + dept.name;
        lv_label_set_text(name_label, name_text.c_str());

        // 员工数量
        lv_obj_t* count_label = lv_label_create(item);
        std::string count_text = "员工：" + std::to_string(emp_count) + "人";
        lv_label_set_text(count_label, count_text.c_str());
    }
}
// ==================== 新增：部门详情界面 ====================

// 部门详情界面加载函数
void load_dept_detail_screen(int dept_id) {
    if (scr_dept) {
        lv_obj_delete(scr_dept);
        scr_dept = nullptr;
    }

    BaseScreenParts parts = create_base_screen("部门设置");
    scr_dept = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DEPT_DETAIL, &scr_dept);

    // 销毁回调
    lv_obj_add_event_cb(scr_dept, [](lv_event_t* e) {
        scr_dept = nullptr;
        ta_dept_name = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建表单容器（使用现成的 create_form_container）
    lv_obj_t* form_cont = create_form_container(parts.content);

    // 部门名称输入（使用现成的 create_form_input）
    ta_dept_name = create_form_input(form_cont, "部门名称:", "请输入部门名称", nullptr, false);

    // 排班设置标题
    lv_obj_t* schedule_title = lv_label_create(form_cont);
    lv_label_set_text(schedule_title, "周排班设置 (0=休息，输入班次 ID):");
    lv_obj_set_style_text_color(schedule_title, THEME_COLOR_TEXT_MAIN, 0);

    // 星期标签数组
    const char* days[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

    // 创建 7 天的输入框（使用 create_form_input）
    for (int i = 0; i < 7; ++i) {
        std::string label_text = std::string(days[i]) + ":";
        lv_obj_t* input = create_form_input(form_cont, label_text.c_str(), "0", nullptr, false);
        ta_schedule_inputs[i] = input;
        
        // 限制只能输入 1 位数字
        lv_textarea_set_max_length(input, 1);
        lv_textarea_set_accepted_chars(input, "0123456789");
    }

    // 创建保存按钮（使用现成的 create_form_btn）
    lv_obj_t* btn_save = create_form_btn(form_cont, "保存", [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED || lv_event_get_key(e) == LV_KEY_ENTER) {
            lv_indev_wait_release(lv_indev_get_act());
            
            // TODO: 读取输入框数据并保存到数据库
            const char* name = lv_textarea_get_text(ta_dept_name);
            if (!name || strlen(name) == 0) {
                show_popup_msg("提示", "部门名称不能为空!", ta_dept_name, "确定");
                return;
            }

            // 示例：读取周日的班次
            const char* sun_shift = lv_textarea_get_text(ta_schedule_inputs[0]);
            
            show_popup_msg("成功", "部门信息已保存！", nullptr, "确定");
            
            // 延迟返回主菜单
            lv_timer_create([](lv_timer_t* t){
                load_att_design_menu_screen();
                lv_timer_del(t);
            }, 1500, nullptr);
        }
    }, nullptr);

    // 将输入框和按钮加入输入组
    UiManager::getInstance()->addObjToGroup(ta_dept_name);
    for (auto& input : ta_schedule_inputs) {
        UiManager::getInstance()->addObjToGroup(input);
    }
    UiManager::getInstance()->addObjToGroup(btn_save);

    // 默认聚焦第一个输入框
    lv_group_focus_obj(ta_dept_name);

    // ESC 返回处理
    lv_obj_add_event_cb(scr_dept, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            lv_indev_wait_release(lv_indev_get_act());
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->addObjToGroup(scr_dept);

    lv_screen_load(scr_dept);
    UiManager::getInstance()->destroyAllScreensExcept(scr_dept);
}


// 部门设置按钮事件回调
static void dept_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    uint32_t key = 0;
    if (code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // ESC 返回
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act());
        load_att_design_menu_screen();
        return;
    }

    // 按钮点击
    if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        lv_indev_wait_release(lv_indev_get_act());

        if (strcmp(tag, "ADD") == 0) {
            // 新增部门 - 使用简单弹窗提示
            show_popup_msg("新增部门", "请输入部门名称（功能开发中）", nullptr, "确定");
        }
        else if (strcmp(tag, "EDIT") == 0) {
            // ✅ 修改部分：调用新的详情界面
            if (g_selected_dept_id < 0) {
                show_popup_msg("提示", "请先选择要编辑的部门!", nullptr, "确定");
                return;
            }
            // 直接调用新创建的详情界面函数
            load_dept_detail_screen(g_selected_dept_id);
        }
        else if (strcmp(tag, "DELETE") == 0) {
            if (g_selected_dept_id < 0) {
                show_popup_msg("提示", "请先选择要删除的部门!", nullptr, "确定");
                return;
            }
            show_popup_msg("确认删除", "确定要删除该部门吗？（功能开发中）", nullptr, "确定");
        }
    }
}


void load_dept_screen() {
    if (scr_dept) {
        lv_obj_delete(scr_dept);
        scr_dept = nullptr;
    }

    BaseScreenParts parts = create_base_screen("部门设置");
    scr_dept = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DEPT_SETTING, &scr_dept);

    // 绑定销毁回调 ✅ 清理所有全局变量
    lv_obj_add_event_cb(scr_dept, [](lv_event_t* e) {
        scr_dept = nullptr;
        dept_list_container = nullptr;
        g_selected_dept_id = -1;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 主容器（85% 高度）
    lv_obj_t* main_cont = lv_obj_create(parts.content);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(85));
    lv_obj_align(main_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    // 部门列表容器
    dept_list_container = lv_obj_create(main_cont);
    lv_obj_set_size(dept_list_container, lv_pct(95), lv_pct(90));
    lv_obj_set_flex_flow(dept_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dept_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(dept_list_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(dept_list_container, LV_SCROLLBAR_MODE_AUTO);

    // 刷新列表
    refresh_dept_list();

    

    // 按钮容器（15% 高度）
    lv_obj_t* btn_cont = lv_obj_create(parts.content);
    lv_obj_set_size(btn_cont, lv_pct(100), lv_pct(15));
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 三个按钮
    lv_obj_t* btn_add = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_add, 100, 50);
    lv_obj_t* label_add = lv_label_create(btn_add);
    lv_label_set_text(label_add, "新增");
    lv_obj_center(label_add);
    lv_obj_add_event_cb(btn_add, dept_btn_event_cb, LV_EVENT_ALL, (void*)"ADD");

    lv_obj_t* btn_edit = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_edit, 100, 50);
    lv_obj_t* label_edit = lv_label_create(btn_edit);
    lv_label_set_text(label_edit, "编辑");
    lv_obj_center(label_edit);
    lv_obj_add_event_cb(btn_edit, dept_btn_event_cb, LV_EVENT_ALL, (void*)"EDIT");

    lv_obj_t* btn_delete = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_delete, 100, 50);
    lv_obj_t* label_delete = lv_label_create(btn_delete);
    lv_label_set_text(label_delete, "删除");
    lv_obj_center(label_delete);
    lv_obj_add_event_cb(btn_delete, dept_btn_event_cb, LV_EVENT_ALL, (void*)"DELETE");

    // 加入输入组
    UiManager::getInstance()->addObjToGroup(btn_add);
    UiManager::getInstance()->addObjToGroup(btn_edit);
    UiManager::getInstance()->addObjToGroup(btn_delete);

    // ESC 返回
    lv_obj_add_event_cb(scr_dept, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->addObjToGroup(scr_dept);

    lv_screen_load(scr_dept);
    UiManager::getInstance()->destroyAllScreensExcept(scr_dept);
}

// ==================== 班次设置子界面 ====================

// 班次设置事件回调
static void shift_set_event_cb(lv_event_t *e) {

    lv_indev_wait_release(lv_indev_get_act());

    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = (code == LV_EVENT_KEY) ? lv_event_get_key(e) : 0;

    // 1. 导航逻辑 (兼容键盘方向键)
    if (code == LV_EVENT_KEY) {
        if (key == LV_KEY_ESC) {
            lv_indev_wait_release(lv_indev_get_act()); // 防连跳
            load_att_design_menu_screen(); // 返回主菜单
            return; // 拦截返回
        } else if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());
            return;
        } else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());
            return;
        }
    }

    // 2. 跳转详情页逻辑 (点击 或 聚焦按回车)
    if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {

        lv_obj_t* target = (lv_obj_t*)lv_event_get_current_target(e); 
        
        // 如果触发事件的是屏幕本身 (比如上一个界面的Enter残留)，直接忽略！
        if (target == scr_shift_set) {
            return; 
        }

        int shift_id = 0;

        // 【途径一】：检查是否通过 lv_obj_add_event_cb 的 user_data 传入（对应 create_sys_list_btn）
        void* event_user_data = lv_event_get_user_data(e);
        if (event_user_data != nullptr) {
            shift_id = (int)(intptr_t)event_user_data;
        } 
        // 【途径二】：检查是否通过 lv_obj_set_user_data 挂载在对象上（对应 lv_btn_create 动态 new 出来的情况）
        else {
            int* p_id = (int*)lv_obj_get_user_data(target);
            if (p_id != nullptr) {
                shift_id = *p_id;
            }
        }

        // 验证拿到真实的 shift_id 后跳转
        if (shift_id > 0) {
            lv_indev_wait_release(lv_indev_get_act()); // 必须在确认要跳转时才清空输入设备状态
            load_shift_info_screen(shift_id);
        } else {
            // DEBUG: 如果没传参成功或者跑到屏幕自身事件上去了，弹窗提示以便排错
            show_popup_msg("错误", "获取班次ID失败,请检查按钮创建逻辑", nullptr, "确认");
        }
    }
}

//班次设置界面
void load_shift_set_screen() {
    if (scr_shift_set){
        lv_obj_delete(scr_shift_set);
        scr_shift_set = nullptr;
    }

    BaseScreenParts parts = create_base_screen("班次设置");
    scr_shift_set = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SHIFT_SETTING, &scr_shift_set);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_shift_set, [](lv_event_t * e) {
        scr_shift_set = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    //循环创建按钮
    std::vector<ShiftInfo> shifts = UiController::getInstance()->getAllShifts();
    
    for (const auto& shift : shifts) {
        // 判断班次是否有排班信息
        // 根据用户说明：一个班次有三个时间段，只要有一个时间段输入了有正确的数据（需要有开始和结束也就是上班和下班）就可以判断为已设置排班信息
        bool has_schedule = false;
        
        // 检查第一个时间段
        if (!shift.s1_start.empty() && !shift.s1_end.empty()) {
            has_schedule = true;
        }
        // 检查第二个时间段
        else if (!shift.s2_start.empty() && !shift.s2_end.empty()) {
            has_schedule = true;
        }
        // 检查第三个时间段
        else if (!shift.s3_start.empty() && !shift.s3_end.empty()) {
            has_schedule = true;
        }
        
        std::string schedule_status = has_schedule ? "已设置排班" : "未设置排班";
        
        // 创建按钮文本：班次ID + 班次名字 + 排班状态
        // 格式：ID: [id] 名称: [name] 状态: [status]
        // 创建按钮文本：左侧显示班次ID和名称，右侧显示排班状态
        std::string left_text = std::to_string(shift.id) + ". ";
        std::string right_text = shift.name;
        
        // 创建按钮
        void* user_data_id = (void*)(intptr_t)shift.id;
        
        create_sys_list_btn(list,left_text.c_str(), right_text.c_str(), schedule_status.c_str(), shift_set_event_cb, user_data_id);  
    }

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_shift_set);
    UiManager::getInstance()->destroyAllScreensExcept(scr_shift_set);// 加载后销毁其他屏幕，保持资源清晰
}

//班次详细信息界面事件回调
static void shift_info_event_cb(lv_event_t *e) {

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回排班设置界面)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); // 【防连跳核心】 --- IGNORE ---
        load_shift_set_screen(); //返回排班设置界面
        return;
    }

    // ================= 焦点在【时段一输入框】 =================
    if (current_target == g_ta_time_frame1) {
        // 按下回车或↓键，跳到时段二输入框
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_time_frame2);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【时段二输入框】 =================
    else if (current_target == g_ta_time_frame2) {
        // 按下↑键，跳回时段一输入框
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_time_frame1);
            return; 
        }
        // 按下回车或↓键，跳到加班输入框
        else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_overtime);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【加班输入框】 =================
    else if (current_target == g_ta_overtime) {
        // 按下↑键，跳回时段二输入框
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_time_frame2);
            return; 
        }
        // 按下回车或↓键，跳到下载按钮
        else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_a1_amend);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【确认修改按钮】 =================
    else if (current_target == g_btn_a1_amend) {
        // 按下↑键，跳回加班输入框
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_overtime);
            return; 
        }

        // 按下确认修改
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---

            std::string s1_input = lv_textarea_get_text(g_ta_time_frame1);
            std::string s2_input = lv_textarea_get_text(g_ta_time_frame2);
            std::string s3_input = lv_textarea_get_text(g_ta_overtime);

            std::string s1_start, s1_end, s2_start, s2_end, s3_start, s3_end;

            // 格式校验
            if (!validate_and_parse_time(s1_input, s1_start, s1_end) ||
                !validate_and_parse_time(s2_input, s2_start, s2_end) ||
                !validate_and_parse_time(s3_input, s3_start, s3_end)) {
                show_popup_msg("格式错误", "时间必须是 HH:MM-HH:MM 格式\n(时: 00-23, 分: 00-59)\n留空代表不排班", nullptr, "确认");
                return;
            }

            // 逻辑校验 (上班时间必须早于下班时间)
            if (!is_valid_time_range(s1_input) || 
                !is_valid_time_range(s2_input) || 
                !is_valid_time_range(s3_input)) {
                show_popup_msg("逻辑错误", "单个时段内，上班时间\n必须早于下班时间!", nullptr, "确认");
                return; // 拦截，不写入数据库
            }

            // 拆分字符串，准备写入数据库
            // 因为之前的格式校验已经保证了长度绝对是 11，所以这里可以直接安全 substr
            s1_start = s1_input.empty() ? "" : s1_input.substr(0, 5);
            s1_end   = s1_input.empty() ? "" : s1_input.substr(6, 5);
            s2_start = s2_input.empty() ? "" : s2_input.substr(0, 5);
            s2_end   = s2_input.empty() ? "" : s2_input.substr(6, 5);
            s3_start = s3_input.empty() ? "" : s3_input.substr(0, 5);
            s3_end   = s3_input.empty() ? "" : s3_input.substr(6, 5);

            // 写入数据库 
            bool success = UiController::getInstance()->updateShiftInfo(
                g_current_edit_shift_id,
                s1_start, s1_end,
                s2_start, s2_end,
                s3_start, s3_end
            );

            if (success) {
                show_popup_msg("保存成功", "班次信息已更新！", nullptr, "确认");
                // 延时返回班次列表
                lv_timer_create([](lv_timer_t* t){
                    load_shift_set_screen();
                    lv_timer_del(t);
                }, 1000, nullptr);
            } else {
                show_popup_msg("保存失败", "写入数据库发生错误，请重试！", nullptr, "确认");
            }
        }
    }
}

//班次详细信息界面
void load_shift_info_screen(int shift_id) {

    g_current_edit_shift_id = shift_id; // 记录当前所处的班次ID

    if(scr_shift_info){
        lv_obj_delete(scr_shift_info);
        scr_shift_info = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("班次详细信息");
    scr_shift_info = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SHIFT_INFO, &scr_shift_info);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_shift_info, [](lv_event_t * e) {
        scr_shift_info = nullptr;
        g_ta_time_frame1 = nullptr;
        g_ta_time_frame2 = nullptr;
        g_ta_overtime = nullptr;
        g_btn_a1_amend = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    // 绑定全局 ESC 返回事件
    lv_obj_add_event_cb(scr_shift_info, shift_info_event_cb, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->resetKeypadGroup();

    // 创建统一表单容器
    lv_obj_t* form_cont = create_form_container(parts.content);

    // ================== [数据加载] ==================
    auto shift_opt = UiController::getInstance()->getShiftInfo(shift_id);
    std::string s1_text = "", s2_text = "", s3_text = "";
    
    if (shift_opt.has_value()) {
        ShiftInfo info = shift_opt.value();
        // 如果数据库中有数据并且不是无效的 "--:--" 标识，则拼接为 "08:00-12:00" 显示
        if (!info.s1_start.empty() && info.s1_start != "--:--") 
            s1_text = info.s1_start + "-" + info.s1_end;
        if (!info.s2_start.empty() && info.s2_start != "--:--") 
            s2_text = info.s2_start + "-" + info.s2_end;
        if (!info.s3_start.empty() && info.s3_start != "--:--") 
            s3_text = info.s3_start + "-" + info.s3_end;
    }

    // 1. 时段一输入框
    g_ta_time_frame1 = create_form_input(form_cont, "时段一:", "如: 08:00-12:00", s1_text.c_str(), false);
    lv_textarea_set_accepted_chars(g_ta_time_frame1, "0123456789:-"); // 限制仅允许数字和符号
    lv_textarea_set_max_length(g_ta_time_frame1, 11);                 // 限制最大字符数 (HH:MM-HH:MM 即11个字符)
    lv_obj_add_event_cb(g_ta_time_frame1, time_input_format_cb, LV_EVENT_VALUE_CHANGED, nullptr); // 绑定自动补全
    lv_obj_add_event_cb(g_ta_time_frame1, shift_info_event_cb, LV_EVENT_ALL, nullptr);// 绑定全局事件处理
    UiManager::getInstance()->addObjToGroup(g_ta_time_frame1);

    // 2. 时段二输入框
    g_ta_time_frame2 = create_form_input(form_cont, "时段二:", "如: 14:00-18:00", s2_text.c_str(), false);
    lv_textarea_set_accepted_chars(g_ta_time_frame2, "0123456789:-");
    lv_textarea_set_max_length(g_ta_time_frame2, 11);
    lv_obj_add_event_cb(g_ta_time_frame2, time_input_format_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(g_ta_time_frame2, shift_info_event_cb, LV_EVENT_ALL, nullptr);// 绑定全局事件处理
    UiManager::getInstance()->addObjToGroup(g_ta_time_frame2);

    // 3. 加班输入框
    g_ta_overtime = create_form_input(form_cont, "加班:", "为空则不显示", s3_text.c_str(), false);
    lv_textarea_set_accepted_chars(g_ta_overtime, "0123456789:-");
    lv_textarea_set_max_length(g_ta_overtime, 11);
    lv_obj_add_event_cb(g_ta_overtime, time_input_format_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(g_ta_overtime, shift_info_event_cb, LV_EVENT_ALL, nullptr);// 绑定全局事件处理
    UiManager::getInstance()->addObjToGroup(g_ta_overtime);

    // 4. 确认修改按钮
    g_btn_a1_amend = create_form_btn(form_cont, "确认下载", shift_info_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_a1_amend, shift_info_event_cb, LV_EVENT_KEY, nullptr); 
    UiManager::getInstance()->addObjToGroup(g_btn_a1_amend);

    // 默认聚焦在开始时间
    lv_group_focus_obj(g_ta_time_frame1);

    lv_screen_load(scr_shift_info);
    UiManager::getInstance()->destroyAllScreensExcept(scr_shift_info);
}


// ==================== 考勤规则子界面 ====================

// 考勤规则子界面按钮事件回调
static void rule_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler(); // 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler(); // 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增班次逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改班次逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除班次逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            }
        }
    }
}

void load_rule_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_rule) {
        lv_obj_delete(scr_rule);
        scr_rule = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("考勤规则");
    scr_rule = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::RULE_SETTING, &scr_rule);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_rule, [](lv_event_t *e) {
        scr_rule = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增规则", rule_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改规则", rule_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除规则", rule_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_rule, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_rule);

    // 加载与清理
    lv_screen_load(scr_rule);
    UiManager::getInstance()->destroyAllScreensExcept(scr_rule);
}

// ==================== 排班设置子界面 ====================

// 排班设置子界面按钮事件回调
static void schedule_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler(); // 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler(); // 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增排班逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改排班逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除排班逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            }
        }
    }
}

void load_schedule_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_schedule) {
        lv_obj_delete(scr_schedule);
        scr_schedule = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("排班设置");
    scr_schedule = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SCHEDULE_SETTING, &scr_schedule);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_schedule, [](lv_event_t *e) {
        scr_schedule = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增排班", schedule_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改排班", schedule_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除排班", schedule_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_schedule, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_schedule);

    // 加载与清理
    lv_screen_load(scr_schedule);
    UiManager::getInstance()->destroyAllScreensExcept(scr_schedule);
}

// ==================== 公司设置子界面 ====================

// 公司设置子界面按钮事件回调
// 公司设置子界面按钮事件回调 ✅
// 公司设置子界面按钮事件回调 [统一风格]
static void company_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    
    uint32_t key = 0;
    if (code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // ESC 返回
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act());
        load_att_design_menu_screen();
        return;
    }

    // 判断是哪个控件
    if (target == ta_company_save) {
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(btn_company_save);
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    else if (target == btn_company_save) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(ta_company_save);
            return;
        }

        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            if (!ta_company_save) return;

            const char* name_str = lv_textarea_get_text(ta_company_save);

            if (name_str == nullptr || strlen(name_str) == 0) {
                show_popup_msg("保存失败", "公司名称不能为空!", ta_company_save, "我知道了");
                return;
            }

            bool success = UiController::getInstance()->saveCompanyName(name_str);

            if (success) {
                show_popup_msg("保存成功", "公司名称已保存!", nullptr, "确认");
                lv_timer_create([](lv_timer_t* t){
                    load_att_design_menu_screen();
                    lv_timer_del(t);
                }, 1500, nullptr);
            } else {
                show_popup_msg("保存失败", "请重试!", ta_company_save, "我知道了");
            }
        }
    }
}
// 公司设置界面创建 
void load_company_screen() {
    // 1. 清理旧屏幕 
    if (scr_company){
        lv_obj_delete(scr_company);
        scr_company = nullptr;
    }

    // 2. 创建屏幕并注册 
    BaseScreenParts parts = create_base_screen("公司设置");
    scr_company = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::COMPANY_SETTING, &scr_company);

    // 3. 绑定销毁回调 
    lv_obj_add_event_cb(scr_company, [](lv_event_t * e) {
        scr_company = nullptr;
        ta_company_save = nullptr;
        btn_company_save = nullptr;
    }, LV_EVENT_DELETE, NULL);

    // 4. 重置输入组 
    UiManager::getInstance()->resetKeypadGroup();

    // 5. 创建表单容器 
    lv_obj_t* form_cont = create_form_container(parts.content);

    // 6. 加载当前数据 
    std::string name;
    UiController::getInstance()->loadCompanyName(name);

    // 7. 创建公司名称输入框 
    ta_company_save = create_form_input(form_cont, "公司名称:", "请输入公司名称", name.c_str(), false);
    
    // 8. 创建保存按钮 
    btn_company_save = create_form_btn(form_cont, "保存", company_btn_event_cb, nullptr);

    // 9. 绑定事件回调 
    lv_obj_add_event_cb(ta_company_save, company_btn_event_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_event_cb(btn_company_save, company_btn_event_cb, LV_EVENT_ALL, nullptr);

    // 10. 加入焦点组 
    UiManager::getInstance()->addObjToGroup(ta_company_save);
    UiManager::getInstance()->addObjToGroup(btn_company_save);

    // 11. 设置默认焦点 
    lv_group_focus_obj(ta_company_save);

    // 12. 加载屏幕 
    lv_screen_load(scr_company);
    UiManager::getInstance()->destroyAllScreensExcept(scr_company);
}

// ==================== 定时响铃子界面 ====================

// 定时响铃子界面按钮事件回调
static void bell_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1 列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler(); // 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler(); // 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增响铃逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改响铃逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除响铃逻辑
                show_popup_msg("班次设置", "该功能正在开发中!\n尽请期待! ", nullptr, "我知道了");
            }
        }
    }
}

void load_bell_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_bell) {
        lv_obj_delete(scr_bell);
        scr_bell = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("定时响铃");
    scr_bell = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::BELL_SETTING, &scr_bell); // 需在 ui_manager.h 中添加枚举

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_bell, [](lv_event_t *e) {
        scr_bell = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增响铃", bell_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改响铃", bell_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除响铃", bell_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_bell, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_bell);

    // 加载与清理
    lv_screen_load(scr_bell);
    UiManager::getInstance()->destroyAllScreensExcept(scr_bell);
}
} // namespace att_design
} // namespace ui
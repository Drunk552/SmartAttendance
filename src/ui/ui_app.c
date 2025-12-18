/**
 * @file ui_app.c
 * @brief UI 层 - v2.1 (Focus Debug Mode)
 * @details 增加焦点移动日志 + 极高对比度的焦点样式
 */
#include "ui_app.h"
#include <lvgl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "../business/face_demo.h" 
#include "lv_conf.h"

// ================= 宏定义 =================
#define SCREEN_W 240
#define SCREEN_H 320
#define CAM_W 240
#define CAM_H 180

// ================= 全局变量 =================
static lv_obj_t *screen_main; 
static lv_obj_t *screen_menu; 
static lv_obj_t *obj_grid; 
static lv_group_t *g_keypad_group; 

// [Epic 3.3 新增]
static lv_obj_t *screen_list;    // 列表页屏幕
static lv_obj_t *obj_list_view;  // 列表控件容器
// [Epic 3.3 注册向导] 全局变量
static lv_obj_t *screen_register;
static lv_obj_t *ta_name;      // 名字输入框
static lv_obj_t *img_face_reg; // 注册页面的摄像头预览
static char g_reg_name[64];    // 暂存输入的名字
// [Epic 3.4 新增] 考勤记录页
static lv_obj_t *screen_records;
static lv_obj_t *obj_record_list;
static void create_record_screen(void);
static void load_record_screen(void);

// 摄像头相关
static uint8_t cam_buf[CAM_W * CAM_H * 3]; 
static lv_obj_t *img_camera = NULL;

#if LV_VERSION_CHECK(9,0,0)
    static lv_image_dsc_t img_dsc;
#else
    static lv_img_dsc_t img_dsc;
#endif

static lv_obj_t *label_time;

// ================= 样式定义 =================
static lv_style_t style_base;
static lv_style_t style_menu_btn;
static lv_style_t style_menu_btn_focused; // 焦点样式

// ================= 声明 =================
static void create_main_screen(void);
static void create_menu_screen(void);
static void load_main_screen(void);
static void load_menu_screen(void);

// [Epic 3.3 新增]
static void create_user_list_screen(void);
static void load_user_list_screen(void);
static void list_btn_event_cb(lv_event_t *e);
// [Epic 3.3 注册向导] 函数声明
static void create_register_screen(void);
static void load_register_step1(void); // 输入姓名
static void load_register_step2(void); // 采集人脸
static void register_face_timer_cb(lv_timer_t *timer); // 注册页面的摄像头刷新

// ================= 辅助函数 =================

static void request_exit(void) {
    printf("[UI] Requesting Exit...\n");
    g_program_should_exit = true; 
}

static void get_current_time_str(char *buf, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buf, size, "%H:%M", timeinfo);
}

static void camera_timer_cb(lv_timer_t *timer) {
    if (g_program_should_exit) return; 
    if (lv_screen_active() == screen_main && img_camera) {
        if (business_get_display_frame(cam_buf, CAM_W, CAM_H)) {
            lv_obj_invalidate(img_camera);
        }
    }
}

static void time_timer_cb(lv_timer_t *timer) {
    if (g_program_should_exit) return;
    if (label_time && lv_obj_is_valid(label_time)) {
        char buf[16];
        get_current_time_str(buf, sizeof(buf));
        lv_label_set_text(label_time, buf);
    }
}

// ================= 事件处理 =================

static void main_screen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) load_menu_screen();
        if (key == LV_KEY_ESC) request_exit();
    }
}

// [升级版] 支持真正的上下左右二维导航
static void menu_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    // 仅处理按键事件
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
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
            printf("[UI] Nav: RIGHT (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_LEFT) {
            // 向左：-1，循环 (加 total 防止负数)
            next_index = (index + total - 1) % total;
            printf("[UI] Nav: LEFT (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_DOWN) {
            // 向下：+2 (因为是2列布局)，循环
            next_index = (index + 2) % total;
            printf("[UI] Nav: DOWN (%d -> %d)\n", index, next_index);
        }
        else if (key == LV_KEY_UP) {
            // 向上：-2，循环
            next_index = (index + total - 2) % total;
            printf("[UI] Nav: UP (%d -> %d)\n", index, next_index);
        }

        // --- 执行跳转 ---
        if (next_index >= 0) {
            // 找到目标按钮
            lv_obj_t *target_btn = lv_obj_get_child(grid, next_index);
            // 强制聚焦
            lv_group_focus_obj(target_btn);
            return; // 完成跳转，直接返回
        }

        // --- 处理功能键 ---
        if (key == LV_KEY_ESC) {
            printf("[UI] ESC -> Back\n");
            load_main_screen();
        }
        else if (key == LV_KEY_ENTER) {
            printf("[UI] Action: %s\n", tag);
            
            if(strcmp(tag, "Users") == 0) {
                load_user_list_screen();
            }
            else if(strcmp(tag, "Records") == 0) {
                 // Records 按钮才应该跳转到 记录页
                 load_record_screen();
            }
            // [修改点] 将 Settings 按钮作为 注册向导 入口
            else if(strcmp(tag, "Settings") == 0) {
                 load_register_step1(); // 跳转到记录页
            }
            else if(strcmp(tag, "System") == 0) {
                 request_exit();
            }
        }
    }
    
    // 保留点击支持
    if (code == LV_EVENT_CLICKED) {
         printf("[UI] Click: %s\n", tag);
         if(strcmp(tag, "System") == 0) {
            extern volatile bool g_program_should_exit;
            g_program_should_exit = true; 
        }  
    }
}

// ================= 初始化 =================

static void init_styles(void) {
    lv_style_init(&style_base);
    lv_style_set_bg_color(&style_base, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&style_base, LV_OPA_COVER);
    lv_style_set_text_color(&style_base, lv_color_hex(0xFFFFFF));

    // 普通按钮：深灰色
    lv_style_init(&style_menu_btn);
    lv_style_set_bg_color(&style_menu_btn, lv_color_hex(0x444444));
    lv_style_set_bg_opa(&style_menu_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_menu_btn, 8);
    lv_style_set_layout(&style_menu_btn, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_menu_btn, LV_FLEX_FLOW_COLUMN);

    // [调试] 焦点样式：鲜艳的红色背景，黄色文字
    lv_style_init(&style_menu_btn_focused);
    lv_style_set_bg_color(&style_menu_btn_focused, lv_palette_main(LV_PALETTE_RED)); // 红色背景
    lv_style_set_bg_opa(&style_menu_btn_focused, LV_OPA_COVER);
    lv_style_set_border_width(&style_menu_btn_focused, 4); 
    lv_style_set_border_color(&style_menu_btn_focused, lv_palette_main(LV_PALETTE_YELLOW)); // 黄色边框
    lv_style_set_text_color(&style_menu_btn_focused, lv_palette_main(LV_PALETTE_YELLOW));   // 黄色文字
}

static void create_main_screen(void) {
    screen_main = lv_obj_create(NULL);
    lv_obj_add_style(screen_main, &style_base, 0);
    lv_obj_set_scrollbar_mode(screen_main, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(screen_main, main_screen_event_cb, LV_EVENT_ALL, NULL);

    // Top Bar
    lv_obj_t *top = lv_obj_create(screen_main);
    lv_obj_set_size(top, SCREEN_W, 30);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);

    label_time = lv_label_create(top);
    lv_label_set_text(label_time, "00:00");
    lv_obj_align(label_time, LV_ALIGN_LEFT_MID, 5, 0);

    // Camera
    img_dsc.header.w = CAM_W;
    img_dsc.header.h = CAM_H;
    img_dsc.data = cam_buf;
    img_dsc.data_size = sizeof(cam_buf);
    #if LV_VERSION_CHECK(9,0,0)
        img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        img_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
        img_dsc.header.stride = CAM_W * 3;
        img_camera = lv_image_create(screen_main);
        lv_image_set_src(img_camera, &img_dsc);
    #else
        img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        img_camera = lv_img_create(screen_main);
        lv_img_set_src(img_camera, &img_dsc);
    #endif
    lv_obj_set_size(img_camera, CAM_W, CAM_H);
    lv_obj_align(img_camera, LV_ALIGN_TOP_MID, 0, 40);

    // Bottom
    lv_obj_t *bottom = lv_obj_create(screen_main);
    lv_obj_set_size(bottom, SCREEN_W, 110);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom, lv_color_hex(0x222222), 0);
    
    lv_obj_t *tip = lv_label_create(bottom);
    lv_label_set_text(tip, "Enter: Menu\nESC: Exit"); 
    lv_obj_set_style_text_color(tip, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(tip, LV_ALIGN_CENTER, 0, 0);
}

static void create_menu_screen(void) {
    screen_menu = lv_obj_create(NULL);
    lv_obj_add_style(screen_menu, &style_base, 0);
    
    lv_obj_t *title = lv_label_create(screen_menu);
    lv_label_set_text(title, "Main Menu");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Grid (增加间距 gap，防止挤压)
    static int32_t col_dsc[] = {100, 100, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {90, 90, LV_GRID_TEMPLATE_LAST}; // 稍微改小高度，留出间隙

    obj_grid = lv_obj_create(screen_menu); 
    lv_obj_set_size(obj_grid, 220, 220); 
    lv_obj_align(obj_grid, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_layout(obj_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj_grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(obj_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj_grid, 0, 0);
    // [重要] 设置Grid间隙，确保边框不重叠
    lv_obj_set_style_pad_column(obj_grid, 10, 0);
    lv_obj_set_style_pad_row(obj_grid, 10, 0);

    const char *labels[] = {"Users", "Records", "Settings", "System"};
    const char *icons[] = {LV_SYMBOL_EDIT, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS, LV_SYMBOL_POWER};
    
    for(int i = 0; i < 4; i++) {
        uint8_t col = i % 2;
        uint8_t row = i / 2;

        lv_obj_t *btn = lv_button_create(obj_grid);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                                  LV_GRID_ALIGN_STRETCH, row, 1);
        
        lv_obj_add_style(btn, &style_menu_btn, 0);
        // 添加 FOCUSED 状态样式 (红色)
        lv_obj_add_style(btn, &style_menu_btn_focused, LV_STATE_FOCUSED);
        // [LVGL9 补充] 确保键盘焦点状态也触发
        lv_obj_add_style(btn, &style_menu_btn_focused, LV_STATE_FOCUS_KEY);

        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, icons[i]);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);

        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_ALL, (void*)labels[i]);
    }
}

// ================= 页面切换 =================

static void load_main_screen(void) {
    if (!screen_main) create_main_screen();
    
    printf("[UI] Switch to Main\n");
    lv_group_remove_all_objs(g_keypad_group);

    #if LV_VERSION_CHECK(9,0,0)
        lv_screen_load(screen_main);
    #else
        lv_scr_load(screen_main);
    #endif

    lv_group_add_obj(g_keypad_group, screen_main);
    lv_group_focus_obj(screen_main);
}

static void load_menu_screen(void) {
    if (!screen_menu) create_menu_screen();

    printf("[UI] Switch to Menu\n");
    lv_group_remove_all_objs(g_keypad_group);

    #if LV_VERSION_CHECK(9,0,0)
        lv_screen_load(screen_menu);
    #else
        lv_scr_load(screen_menu);
    #endif

    // 加入按钮到组
    uint32_t cnt = lv_obj_get_child_cnt(obj_grid);
    for(uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(obj_grid, i);
        lv_group_add_obj(g_keypad_group, child);
    }
    
    // 强制聚焦第一个
    if (cnt > 0) {
        lv_obj_t *first_btn = lv_obj_get_child(obj_grid, 0);
        lv_group_focus_obj(first_btn);
        printf("[UI] Initial Focus Set to First Button\n");
    }
    
    // 兜底背景
    lv_group_add_obj(g_keypad_group, screen_menu);
}

// =================================================================
// [Epic 3.3 新增] 员工列表页实现代码
// =================================================================

// 1. 列表按钮的事件回调 (处理上下滚动和返回)
static void list_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 向下键：移动到下一个
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(g_keypad_group);
        }
        // 向上键：移动到上一个
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(g_keypad_group);
        }
        // ESC键：返回主菜单
        else if (key == LV_KEY_ESC) {
            load_menu_screen();
        }
        // ENTER键：查看详情 (目前仅打印)
        else if (key == LV_KEY_ENTER) {
            printf("[UI] View User Details...\n");
        }
    }
}

// 2. 创建列表屏幕 UI
static void create_user_list_screen(void) {
    screen_list = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_list, lv_color_hex(0x000000), 0);
    
    // 标题
    lv_obj_t *title = lv_label_create(screen_list);
    lv_label_set_text(title, "User List");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 列表控件容器
    obj_list_view = lv_list_create(screen_list);
    lv_obj_set_size(obj_list_view, 220, 260);
    lv_obj_align(obj_list_view, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(obj_list_view, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(obj_list_view, 0, 0);
}

// 3. 加载数据并显示
static void load_user_list_screen(void) {
    if (!screen_list) create_user_list_screen();
    
    printf("[UI] Switch to User List\n");
    
    // A. 清空输入组 (准备接管键盘)
    lv_group_remove_all_objs(g_keypad_group);
    
    // B. 清空旧列表内容
    lv_obj_clean(obj_list_view);
    
    // C. 从业务层获取用户数据
    int count = business_get_user_count(); // 调用 face_demo.cpp 的接口
    printf("[UI] Fetching users: %d\n", count);
    
    if (count == 0) {
        lv_list_add_text(obj_list_view, "No Users Found");
    } else {
        char name_buf[64];
        int id = 0;
        char label_buf[128];
        
        for(int i=0; i<count; i++) {
            if (business_get_user_at(i, &id, name_buf, sizeof(name_buf))) {
                sprintf(label_buf, "[%d] %s", id, name_buf);
                
                // 1. 创建按钮
                lv_obj_t *btn = lv_list_add_button(obj_list_view, LV_SYMBOL_BULLET, label_buf);
                
                // [新增] 2. 检查是否创建成功 (防崩溃)
                if (btn == NULL) {
                    printf("[Error] LVGL Out of Memory! Stopped at user %d\n", i);
                    lv_label_set_text(lv_list_add_text(obj_list_view, "Memory Full!"), "System Out of Memory");
                    break; // 停止继续创建，保护程序不崩
                }

                // 3. 设置事件和组
                lv_obj_add_event_cb(btn, list_btn_event_cb, LV_EVENT_KEY, NULL);
                lv_group_add_obj(g_keypad_group, btn);
                
                if (i == 0) lv_group_focus_obj(btn);
            }
        }
    }
    
    // D. 切换屏幕
    #if LV_VERSION_CHECK(9,0,0)
        lv_screen_load(screen_list);
    #else
        lv_scr_load(screen_list);
    #endif
    
    // E. 兜底：把背景也加入组，防止列表为空时按键失效
    lv_group_add_obj(g_keypad_group, screen_list);
}

// =================================================================
// [Epic 3.3] 注册向导实现 (Wizard Implementation)
// =================================================================

// --- 辅助：注册页面的摄像头刷新 ---
static void register_face_timer_cb(lv_timer_t *timer) {
    // 只有在注册界面才刷新
    if (lv_screen_active() != screen_register) return;
    
    // 复用全局 cam_buf 和业务接口
    if (img_face_reg && business_get_display_frame(cam_buf, CAM_W, CAM_H)) {
        lv_obj_invalidate(img_face_reg);
    }
}

// --- Step 2 事件: 处理按键 (拍照/返回) ---
static void reg_step2_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_ENTER) {
            // [核心] 触发业务层采集
            printf("[UI] Capturing face for: %s\n", g_reg_name);
            
            // 调用业务接口保存
            if (business_register_user(g_reg_name)) {
                printf("[UI] Reg Success! Back to Menu.\n");
                
                // 简单反馈：延迟或直接返回
                load_menu_screen(); 
            } else {
                printf("[UI] Reg Failed!\n");
            }
        }
        else if (key == LV_KEY_ESC) {
            load_register_step1(); // 返回上一步
        }
    }
}

// --- Step 1 事件: 输入姓名完成 ---
static void ta_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 按 Enter 确认名字，进入下一步
        if (key == LV_KEY_ENTER) {
            const char *txt = lv_textarea_get_text(ta_name);
            if (strlen(txt) == 0) return; // 禁止空名
            
            strcpy(g_reg_name, txt);
            printf("[UI] Name confirmed: %s -> Next Step\n", g_reg_name);
            
            load_register_step2(); // 进入拍照步骤
        }
        else if (key == LV_KEY_ESC) {
            load_menu_screen(); // 放弃注册
        }
    }
}

// --- 创建注册屏幕容器 ---
static void create_register_screen(void) {
    if (screen_register) return;
    screen_register = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_register, lv_color_black(), 0);
    
    // 注册专用的定时器 (100ms刷新)，与主页定时器分开
    lv_timer_create(register_face_timer_cb, 100, NULL);
}

// --- 加载 Step 1: 输入姓名 ---
static void load_register_step1(void) {
    create_register_screen();
    
    printf("[UI] Wizard Step 1: Name\n");

    // 1. 清理并准备界面
    lv_obj_clean(screen_register);
    lv_group_remove_all_objs(g_keypad_group);
    
    // 2. 标题
    lv_obj_t *label = lv_label_create(screen_register);
    lv_label_set_text(label, "Step 1: Enter Name");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);
    
    // 3. 输入框
    ta_name = lv_textarea_create(screen_register);
    lv_textarea_set_one_line(ta_name, true);
    lv_obj_set_width(ta_name, 200);
    lv_obj_align(ta_name, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_event_cb(ta_name, ta_event_cb, LV_EVENT_ALL, NULL);
    
    // 4. 提示
    lv_obj_t *tip = lv_label_create(screen_register);
    lv_label_set_text(tip, "Type name & Press ENTER");
    lv_obj_set_style_text_color(tip, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -40);
    
    // 5. 切换屏幕与焦点
    #if LV_VERSION_CHECK(9,0,0)
        lv_screen_load(screen_register);
    #else
        lv_scr_load(screen_register);
    #endif
    
    // 将输入框加入组，让物理键盘能输入
    lv_group_add_obj(g_keypad_group, ta_name);
    lv_group_focus_obj(ta_name);
}

// --- 加载 Step 2: 采集人脸 ---
static void load_register_step2(void) {
    printf("[UI] Wizard Step 2: Face\n");

    // 1. 清理 Step 1 的控件
    lv_obj_clean(screen_register);
    lv_group_remove_all_objs(g_keypad_group);
    
    // 2. 标题
    lv_obj_t *label = lv_label_create(screen_register);
    lv_label_set_text(label, "Step 2: Capture Face");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    
    // 3. 摄像头预览区域 (复用全局 dsc)
    #if LV_VERSION_CHECK(9,0,0)
        img_face_reg = lv_image_create(screen_register);
        lv_image_set_src(img_face_reg, &img_dsc); 
    #else
        img_face_reg = lv_img_create(screen_register);
        lv_img_set_src(img_face_reg, &img_dsc);
    #endif
    lv_obj_set_size(img_face_reg, CAM_W, CAM_H);
    lv_obj_align(img_face_reg, LV_ALIGN_CENTER, 0, 0);
    
    // 给图片加个绿色边框，表示这是取景框
    lv_obj_set_style_border_width(img_face_reg, 3, 0);
    lv_obj_set_style_border_color(img_face_reg, lv_palette_main(LV_PALETTE_GREEN), 0);
    
    // 4. 将图片设为可交互，用来接收 ENTER 键
    lv_obj_add_flag(img_face_reg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img_face_reg, reg_step2_event_cb, LV_EVENT_KEY, NULL);
    
    // 5. 提示
    lv_obj_t *tip = lv_label_create(screen_register);
    lv_label_set_text_fmt(tip, "Hi, %s!\nPress ENTER to Capture", g_reg_name);
    lv_obj_set_style_text_color(tip, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // 6. 焦点设置
    lv_group_add_obj(g_keypad_group, img_face_reg);
    lv_group_focus_obj(img_face_reg);
}

// ================= 初始化 =================

void ui_init(void) {
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
    (void)disp;
    
    lv_sdl_mouse_create();

    // 创建键盘设备
    lv_indev_t *kbd = lv_sdl_keyboard_create();
    
    if (kbd) {
        // [核心修复] 强制将键盘类型设为 Keypad (考勤机模式)
        // 这样 LVGL 才会把 方向键 当作 焦点切换键
        lv_indev_set_type(kbd, LV_INDEV_TYPE_KEYPAD);
        printf("[UI] Keyboard force set to KEYPAD mode.\n");
    }

    // 1. 确保创建了组
    g_keypad_group = lv_group_create();

    // 2. [重要] 开启循环模式 (Wrap)
    // 这样按到最后一个图标时，再按右键会自动跳回第一个
    lv_group_set_wrap(g_keypad_group, true);

    if (kbd) lv_indev_set_group(kbd, g_keypad_group);

    init_styles();
    create_main_screen();
    
    lv_timer_create(camera_timer_cb, 100, NULL);
    lv_timer_create(time_timer_cb, 1000, NULL);

    load_main_screen();
    printf("[UI] Debug Mode v2.1 (Red Focus Style)\n");
}

// =================================================================
// [Epic 3.4] 考勤记录页 (Record List)
// =================================================================

static void create_record_screen(void) {
    if (screen_records) return;
    
    screen_records = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_records, lv_color_hex(0x000000), 0);
    
    // 标题
    lv_obj_t *title = lv_label_create(screen_records);
    lv_label_set_text(title, "Attendance Log");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 列表容器
    obj_record_list = lv_list_create(screen_records);
    lv_obj_set_size(obj_record_list, 220, 260);
    lv_obj_align(obj_record_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(obj_record_list, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(obj_record_list, 0, 0);
}

static void load_record_screen(void) {
    create_record_screen();
    
    printf("[UI] Loading Records...\n");
    
    // 1. 准备界面
    lv_group_remove_all_objs(g_keypad_group);
    lv_obj_clean(obj_record_list);
    
    // 2. 获取数据
    int count = business_get_record_count();
    printf("[UI] Found %d records\n", count);
    
    if (count == 0) {
        lv_list_add_text(obj_record_list, "No Records Found");
    } else {
        char buf[128];
        for(int i=0; i<count; i++) {
            if (business_get_record_at(i, buf, sizeof(buf))) {
                // 使用列表文本模式，前面加个时钟图标
                lv_obj_t *btn = lv_list_add_button(obj_record_list, LV_SYMBOL_LIST, buf);
                
                // 复用之前的列表按键回调 (处理上下滚动/ESC返回)
                lv_obj_add_event_cb(btn, list_btn_event_cb, LV_EVENT_KEY, NULL);
                lv_group_add_obj(g_keypad_group, btn);
                
                if (i==0) lv_group_focus_obj(btn);
            }
        }
    }
    
    // 3. 切换显示
    #if LV_VERSION_CHECK(9,0,0)
        lv_screen_load(screen_records);
    #else
        lv_scr_load(screen_records);
    #endif
    
    // 4. 兜底焦点
    lv_group_add_obj(g_keypad_group, screen_records);
}
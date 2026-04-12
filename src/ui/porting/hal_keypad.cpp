#include "hal_keypad.h"
#include "../common/ime_chinese.h"
//Deleted:#include <wiringPi.h>
#ifdef RPI_BUILD
#include <wiringPi.h>
#endif
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

// 引用中文字体（在 font_noto_16.c 中定义）
LV_FONT_DECLARE(font_noto_16);

// ========================== GPIO 引脚定义 ==========================

// Row 1~4: BCM引脚（物理 Pin 7, 29, 31, 33）
//Deleted:const int ROW_PINS[4] = {4, 5, 6, 13};
// Col 1~4: BCM引脚（物理 Pin 32, 36, 38, 37）
//Deleted:const int COL_PINS[4] = {12, 16, 20, 26};
#ifdef RPI_BUILD
const int ROW_PINS[4] = {4, 5, 6, 13};
const int COL_PINS[4] = {12, 16, 20, 26};
#endif

// ========================== 输入模式全局状态 ==========================

static HalInputMode g_input_mode = HalInputMode::NUMERIC;
static void (*g_mode_change_cb)(HalInputMode) = nullptr;

HalInputMode hal_keypad_get_mode() {
    return g_input_mode;
}

void hal_keypad_set_mode(HalInputMode mode) {
    g_input_mode = mode;
    if (g_mode_change_cb) {
        g_mode_change_cb(g_input_mode);
    }
}

const char* hal_keypad_get_mode_str() {
    switch (g_input_mode) {
        case HalInputMode::NUMERIC: return "123";
        case HalInputMode::ENGLISH: return "ABC";
        case HalInputMode::CHINESE: return "\xe6\x8b\xbc"; // UTF-8 "拼"
        default:                    return "123";
    }
}

void hal_keypad_set_mode_change_cb(void (*cb)(HalInputMode new_mode)) {
    g_mode_change_cb = cb;
}

// ========================== 中文候选字条全局状态 ==========================

static lv_obj_t* g_candidate_bar = nullptr; // 当前屏幕的候选字条控件
static lv_obj_t* g_target_ta     = nullptr; // 当前聚焦的 TextArea

void hal_keypad_set_candidate_bar(lv_obj_t* bar) {
    g_candidate_bar = bar;
    // 切换屏幕时同步重置拼音缓冲
    ime_cn_reset();
    // 如果注销（传 nullptr），保持候选条不可见
    if (!bar) return;
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
}

void hal_keypad_set_target_textarea(lv_obj_t* ta) {
    g_target_ta = ta;
}

// ========================== 防抖 ==========================

#define DEBOUNCE_MS 20
static uint32_t g_last_press_tick  = 0;
static uint32_t g_last_key         = 0;  // LVGL 要求松开时也回传上次键值
static int      g_prev_key_index   = -1; // 上次扫描到的按键索引（用于边沿检测）

// ========================== T9 英文连按逻辑 ==========================

/*
 * 标准 T9 映射：数字键 2~9 对应字母列表
 * 0 键 = 英文模式下输出 '-'（减号），1 键 = 无字母
 */
static const char* T9_LETTERS[10] = {
    "-",     // 0: 减号（英文模式）
    "",      // 1: 无
    "abc",   // 2
    "def",   // 3
    "ghi",   // 4
    "jkl",   // 5
    "mno",   // 6
    "pqrs",  // 7
    "tuv",   // 8
    "wxyz"   // 9
};

/*
 * 连按超时时间 (ms): 同一键连按需在此时间内
 * 超出该时间后，LVGL 会将当前字母提交，下次按键重新开始
 */
#define T9_TIMEOUT_MS 1000

static int      g_t9_digit       = -1;  // 当前正在输入的数字键 (0~9)
static int      g_t9_press_count =  0;  // 同一键连按次数
static uint32_t g_t9_last_tick   =  0;  // 上次按下同一键的时间戳

/*
 * @brief 获取当前英文 T9 应输出的字符
 *        如果当前模式不是 ENGLISH，返回 0 表示无效
 */
static uint32_t t9_get_current_char() {
    if (g_t9_digit < 0 || g_t9_digit > 9) return 0;
    const char* letters = T9_LETTERS[g_t9_digit];
    if (!letters || letters[0] == '\0') return 0;
    int len = (int)strlen(letters);
    int idx = g_t9_press_count % len;
    return (uint32_t)(unsigned char)letters[idx];
}

// ========================== 输入模式切换 ==========================

// -------- 中文候选字条内部刷新逻辑 --------

/*
 * 候选字条布局：
 *   [ 拼音提示 | 候1 | 候2 | 候3 | ... ]
 *
 * 每个候选字是一个透明背景的 Label，用户通过 ↑/↓ 选中（高亮）后按 OK 插入。
 * g_cn_selected_idx 记录当前高亮的候选字索引。
 */
static int g_cn_selected_idx = 0; // 当前高亮的候选字索引

// 候选字条内子控件索引约定：
//   child 0 = 拼音提示 Label
//   child 1..N = 候选字 Label

static void candidate_bar_refresh() {
    if (!g_candidate_bar) return;

    // 1. 清空所有现有子控件
    lv_obj_clean(g_candidate_bar);
    g_cn_selected_idx = 0;

    std::vector<std::string> cands = ime_cn_get_candidates();
    std::string pinyin_hint = ime_cn_get_pinyin_display();

    if (cands.empty() && pinyin_hint.empty()) {
        // 没有候选字也没有拼音提示 → 隐藏候选条
        lv_obj_add_flag(g_candidate_bar, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 2. 显示候选条
    lv_obj_remove_flag(g_candidate_bar, LV_OBJ_FLAG_HIDDEN);

    // 3. 创建拼音提示标签（灰色小字）
    lv_obj_t* lbl_py = lv_label_create(g_candidate_bar);
    lv_label_set_text(lbl_py, pinyin_hint.c_str());
    lv_obj_set_style_text_font(lbl_py, &font_noto_16, 0); // 必须设置中文字体
    lv_obj_set_style_text_color(lbl_py, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_bg_opa(lbl_py, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lbl_py, 0, 0);
    lv_obj_set_style_pad_hor(lbl_py, 4, 0);
    lv_obj_set_style_pad_ver(lbl_py, 0, 0);

    // 4. 创建候选字标签（每个汉字一个 Label）
    for (int i = 0; i < (int)cands.size(); i++) {
        lv_obj_t* lbl = lv_label_create(g_candidate_bar);
        lv_label_set_text(lbl, cands[i].c_str());
        lv_obj_set_style_text_font(lbl, &font_noto_16, 0); // 必须设置中文字体
        lv_obj_set_style_pad_hor(lbl, 8, 0);
        lv_obj_set_style_pad_ver(lbl, 0, 0);
        lv_obj_set_style_border_width(lbl, 0, 0);
        lv_obj_set_style_radius(lbl, 0, 0);

        // 默认：白色文字，透明背景
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);

        // 第一个候选字默认高亮
        if (i == 0) {
            lv_obj_set_style_bg_color(lbl, lv_color_hex(0x0055FF), 0);
            lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
        }
    }
}

// 移动候选字高亮（方向: +1 向右/-1 向左）
static void candidate_move_select(int dir) {
    if (!g_candidate_bar) return;
    // 子控件: 0=拼音提示, 1..N=候选字
    int child_cnt = (int)lv_obj_get_child_count(g_candidate_bar);
    int cand_cnt = child_cnt - 1; // 减去拼音提示
    if (cand_cnt <= 0) return;

    // 取消当前高亮
    lv_obj_t* cur = lv_obj_get_child(g_candidate_bar, g_cn_selected_idx + 1);
    if (cur) {
        lv_obj_set_style_bg_opa(cur, LV_OPA_TRANSP, 0);
    }

    // 循环移动
    g_cn_selected_idx = (g_cn_selected_idx + dir + cand_cnt) % cand_cnt;

    // 高亮新选中
    lv_obj_t* nxt = lv_obj_get_child(g_candidate_bar, g_cn_selected_idx + 1);
    if (nxt) {
        lv_obj_set_style_bg_color(nxt, lv_color_hex(0x0055FF), 0);
        lv_obj_set_style_bg_opa(nxt, LV_OPA_COVER, 0);
    }
}

// 确认插入当前选中的候选字到目标 TextArea
static void candidate_confirm() {
    if (!g_candidate_bar || !g_target_ta) return;
    int child_cnt = (int)lv_obj_get_child_count(g_candidate_bar);
    int cand_cnt = child_cnt - 1;
    if (cand_cnt <= 0) return;

    lv_obj_t* sel = lv_obj_get_child(g_candidate_bar, g_cn_selected_idx + 1);
    if (!sel) return;

    const char* ch_text = lv_label_get_text(sel);
    if (ch_text && strlen(ch_text) > 0) {
        // 将 UTF-8 汉字（3字节）逐字节插入 textarea
        lv_textarea_add_text(g_target_ta, ch_text);
    }

    // 插入后重置拼音缓冲并隐藏候选条
    ime_cn_reset();
    lv_obj_add_flag(g_candidate_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(g_candidate_bar);
}

static void switch_to_next_mode() {
    switch (g_input_mode) {
        case HalInputMode::NUMERIC: g_input_mode = HalInputMode::ENGLISH; break;
        case HalInputMode::ENGLISH: g_input_mode = HalInputMode::CHINESE; break;
        case HalInputMode::CHINESE: g_input_mode = HalInputMode::NUMERIC; break;
    }
    // 切换模式时重置 T9 英文状态
    g_t9_digit       = -1;
    g_t9_press_count =  0;
    g_t9_last_tick   =  0;
    // 切换模式时重置中文拼音缓冲，隐藏候选条
    ime_cn_reset();
    if (g_candidate_bar) {
        lv_obj_add_flag(g_candidate_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(g_candidate_bar);
    }
    // 通知 UI 层
    if (g_mode_change_cb) {
        g_mode_change_cb(g_input_mode);
    }
}

// ========================== 主键盘映射表 ==========================

/*
 * 4x4 矩阵键盘物理索引 → 数字值
 * 布局 (key_index = row*4 + col, 从0开始):
 *  idx  0  1  2  3  | 第1行: 1  2 ABC  3 DEF  ESC
 *  idx  4  5  6  7  | 第2行: 4 GHI  5 JKL  6 MNO  MENU←
 *  idx  8  9 10 11  | 第3行: 7 PQRS  8 TUV  9 WXYZ  ▲
 *  idx 12 13 14 15  | 第4行: ● #   0 _   OK   ▼
 *
 * 数字键返回对应的数字字符 '0'~'9'，功能键返回特殊其他值。
 * 用 -1 表示"切换模式"，其他负数表示对应 LV_KEY_* 的负数编码。
 *
 * 为避免与 LV_KEY_* 充突，功能键使用负数表示映射类型:
 *   -1  = 切换模式 (● #)
 *   -2  = ESC
 *   -3  = 左导航 / MENU←
 *   -4  = 上方向键
 *   -5  = OK / 回车
 *   -6  = 下方向键
 *   -7  = 退格 (BACKSPACE)
 *  0~9  = 数字键
 */
static const int DIGIT_MAP[16] = {
    /* 0*/  1,   /* 第1行第1列: 数字 1 */
    /* 1*/  2,   /* 第1行第2列: 数字 2 */
    /* 2*/  3,   /* 第1行第3列: 数字 3 */
    /* 3*/ -2,   /* 第1行第4列: ESC */
    /* 4*/  4,   /* 第2行第1列: 数字 4 */
    /* 5*/  5,   /* 第2行第2列: 数字 5 */
    /* 6*/  6,   /* 第2行第3列: 数字 6 */
    /* 7*/ -3,   /* 第2行第4列: MENU← 导航 */
    /* 8*/  7,   /* 第3行第1列: 数字 7 */
    /* 9*/  8,   /* 第3行第2列: 数字 8 */
    /*10*/  9,   /* 第3行第3列: 数字 9 */
    /*11*/ -4,   /* 第3行第4列: ▲ 上方向键 */
    /*12*/ -1,   /* 第4行第1列: ● # 切换模式 */
    /*13*/  0,   /* 第4行第2列: 数字 0 */
    /*14*/ -5,   /* 第4行第3列: OK / 回车 */
    /*15*/ -6,   /* 第4行第4列: ▼ 下方向键 */
};

// ========================== 矩阵扫描 ==========================

// Deleted:static int scan_matrix() {
// Deleted:    for (int r = 0; r < 4; r++) {
// Deleted:        digitalWrite(ROW_PINS[r], LOW);
// Deleted:        delayMicroseconds(50);
// Deleted:        for (int c = 0; c < 4; c++) {
// Deleted:            if (digitalRead(COL_PINS[c]) == LOW) {
// Deleted:                digitalWrite(ROW_PINS[r], HIGH);
// Deleted:                return r * 4 + c;
// Deleted:            }
// Deleted:        }
// Deleted:        digitalWrite(ROW_PINS[r], HIGH);
// Deleted:    }
// Deleted:    return -1;
// Deleted:}
#ifdef RPI_BUILD
static int scan_matrix() {
    for (int r = 0; r < 4; r++) {
        digitalWrite(ROW_PINS[r], LOW);
        delayMicroseconds(50);
        for (int c = 0; c < 4; c++) {
            if (digitalRead(COL_PINS[c]) == LOW) {
                digitalWrite(ROW_PINS[r], HIGH);
                return r * 4 + c;
            }
        }
        digitalWrite(ROW_PINS[r], HIGH);
    }
    return -1;
}
#else
static int scan_matrix() {
    return -1; // SDL 模拟环境：无物理键盘，始终返回无按键
}
#endif

// ========================== LVGL 输入回调 ==========================

static void keypad_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    int key_index = scan_matrix();
    uint32_t now  = lv_tick_get();

    if (key_index != -1) {
        // ---- 边沿检测 + 防抖 ----
        // 只在按键"从无到有"（g_prev_key_index == -1）或"换了一个键"时才触发一次处理。
        // 如果同一个键持续按住，g_prev_key_index == key_index，直接保持 PRESSED 状态返回，
        // 不重复执行 ime_cn_push_digit / T9 逻辑，从根本上防止连续追加。
        bool is_new_press = (key_index != g_prev_key_index);

        if (is_new_press) {
            // 新按键：需要通过防抖时间才能生效
            if ((now - g_last_press_tick) < DEBOUNCE_MS) {
                // 防抖期内，把上次的键当作"仍在按"
                data->state = LV_INDEV_STATE_PRESSED;
                data->key   = g_last_key;
                return;
            }
            g_last_press_tick = now;
            g_prev_key_index  = key_index;
        } else {
            // 同一个键持续按住：直接回传已设定的 g_last_key，不重新处理
            data->state = LV_INDEV_STATE_PRESSED;
            data->key   = g_last_key;
            return;
        }

        int digit_or_func = DIGIT_MAP[key_index];

        // -------- 处理功能键（负数） --------
        if (digit_or_func < 0) {
            // 英文 T9 模式下，按下任何功能键先提交当前 T9 字符
            // (已通过 g_last_key 提交了，下一个周期 LVGL 会自动接收)
            if (g_input_mode == HalInputMode::ENGLISH) {
                g_t9_digit       = -1;
                g_t9_press_count =  0;
            }

            // ---- 中文模式下，部分功能键被候选字交互拦截 ----
            if (g_input_mode == HalInputMode::CHINESE) {
                bool cn_has_candidates = (g_candidate_bar &&
                    !lv_obj_has_flag(g_candidate_bar, LV_OBJ_FLAG_HIDDEN) &&
                    lv_obj_get_child_count(g_candidate_bar) > 1);

                if (digit_or_func == -5 && cn_has_candidates) {
                    // OK：确认插入选中的候选字
                    candidate_confirm();
                    g_last_key = 0;
                    data->state = LV_INDEV_STATE_RELEASED;
                    data->key   = 0;
                    return;
                }
                if (digit_or_func == -4 && cn_has_candidates) {
                    // ▲ 上：候选字向左移动
                    candidate_move_select(-1);
                    g_last_key = 0;
                    data->state = LV_INDEV_STATE_RELEASED;
                    data->key   = 0;
                    return;
                }
                if (digit_or_func == -6 && cn_has_candidates) {
                    // ▼ 下：候选字向右移动
                    candidate_move_select(+1);
                    g_last_key = 0;
                    data->state = LV_INDEV_STATE_RELEASED;
                    data->key   = 0;
                    return;
                }
                if (digit_or_func == -7 && cn_has_candidates) {
                    // BACKSPACE：删除最后一个拼音字母并刷新候选条
                    ime_cn_pop();
                    candidate_bar_refresh();
                    g_last_key = 0;
                    data->state = LV_INDEV_STATE_RELEASED;
                    data->key   = 0;
                    return;
                }
                if (digit_or_func == -2 && cn_has_candidates) {
                    // ESC：取消当前拼音输入，隐藏候选条（不向 LVGL 发送 ESC）
                    ime_cn_reset();
                    lv_obj_add_flag(g_candidate_bar, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clean(g_candidate_bar);
                    g_last_key = 0;
                    data->state = LV_INDEV_STATE_RELEASED;
                    data->key   = 0;
                    return;
                }
            }

            switch (digit_or_func) {
                case -1: switch_to_next_mode(); g_last_key = 0; break; // 切换模式，不发按键
                case -2: g_last_key = LV_KEY_ESC;       break;
                case -3: g_last_key = LV_KEY_LEFT;      break;
                case -4: g_last_key = LV_KEY_UP;        break;
                case -5: g_last_key = LV_KEY_ENTER;     break;
                case -6: g_last_key = LV_KEY_DOWN;      break;
                case -7: g_last_key = LV_KEY_BACKSPACE; break;
                default: g_last_key = 0; break;
            }

            data->state = (g_last_key != 0) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            data->key   = g_last_key;
            return;
        }

        // -------- 处理数字键 (0~9) --------
        int digit = digit_or_func;

        switch (g_input_mode) {

        case HalInputMode::NUMERIC:
            // 数字模式：直接发出 ASCII '0'~'9'
            g_last_key = (uint32_t)('0' + digit);
            break;

        case HalInputMode::ENGLISH: {
            // 英文模式: T9 连按逻辑
            // 如果按了不同的键，之前的字母已通过 g_last_key 提交
            // 现在开始新键序列
            if (digit != g_t9_digit) {
                // 切换到新键，重置计数
                g_t9_digit       = digit;
                g_t9_press_count = 0;
            } else {
                // 同一键连按：判断是否超时
                if ((now - g_t9_last_tick) >= T9_TIMEOUT_MS) {
                    // 超时：上一个字母已提交，重新开始
                    g_t9_press_count = 0;
                } else {
                    // 未超时：切换到下一个字母
                    g_t9_press_count++;
                }
            }
            g_t9_last_tick = now;

            uint32_t ch = t9_get_current_char();
            if (ch != 0) {
                g_last_key = ch; // 例如 'a', 'b', 'c'…
            } else {
                // 数字1 或 没对应字母的数字，备用方案输出数字本身
                g_last_key = (uint32_t)('0' + digit);
            }
            break;
        }

        case HalInputMode::CHINESE:
            // 中文模式: T9 拼音组合
            // 每按一个数字键，追加到拼音缓冲区，刷新候选字条
            ime_cn_push_digit(digit);
            candidate_bar_refresh();
            // 中文模式下数字键本身不向 LVGL 发送任何按键
            g_last_key = 0;
            data->state = LV_INDEV_STATE_RELEASED;
            data->key   = 0;
            return;
        }

        data->state = LV_INDEV_STATE_PRESSED;
        data->key   = g_last_key;

    } else {
        // 没有键被按下：重置边沿检测状态，允许下次按键触发
        g_prev_key_index  = -1;
        data->state = LV_INDEV_STATE_RELEASED;
        data->key   = g_last_key; // LVGL 规范：松开时也要回传上次的键值
    }
}

// ========================== 初始化 ==========================

lv_indev_t * hal_keypad_init(void) {
    printf("[HAL] 初始化矩阵键盘 (wiringPi)...\n");

    // Deleted:    if (wiringPiSetupGpio() == -1) {
    // Deleted:        printf("[HAL] Error: wiringPi 初始化失败!\n");
    // Deleted:        return nullptr;
    // Deleted:    }
    // Deleted:
    // Deleted:    for (int i = 0; i < 4; i++) {
    // Deleted:        pinMode(ROW_PINS[i], OUTPUT);
    // Deleted:        digitalWrite(ROW_PINS[i], HIGH);
    // Deleted:        pinMode(COL_PINS[i], INPUT);
    // Deleted:        pullUpDnControl(COL_PINS[i], PUD_UP);
    // Deleted:    }
    #ifdef RPI_BUILD
        if (wiringPiSetupGpio() == -1) {
            printf("[HAL] Error: wiringPi 初始化失败!\n");
            return nullptr;
        }

        for (int i = 0; i < 4; i++) {
            pinMode(ROW_PINS[i], OUTPUT);
            digitalWrite(ROW_PINS[i], HIGH);
            pinMode(COL_PINS[i], INPUT);
            pullUpDnControl(COL_PINS[i], PUD_UP);
        }
        printf("[HAL] 矩阵键盘初始化完成，默认模式: 数字\n");
    #else
        printf("[HAL] SDL 模拟模式：跳过 GPIO 初始化\n");
    #endif

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keypad_read_cb);

    printf("[HAL] 矩阵键盘初始化完成，默认模式: 数字\n");
    return indev;
}
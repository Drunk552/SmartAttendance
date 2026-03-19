/**
 * @file matrix_t9_keyboard.cpp
 * @brief 矩阵键盘T9映射与输入法逻辑实现
 * @details 为智能考勤系统设计的矩阵键盘T9功能实现
 */

#include "matrix_t9_keyboard.h"
#include <algorithm>
#include <chrono>
#include <cstring>

// 标准T9数字到字母映射
const char* MatrixT9Keyboard::s_digit_to_letters[10] = {
    " ",      // 0: 空格
    "",       // 1: 无字母
    "abc",    // 2: abc
    "def",    // 3: def
    "ghi",    // 4: ghi
    "jkl",    // 5: jkl
    "mno",    // 6: mno
    "pqrs",   // 7: pqrs
    "tuv",    // 8: tuv
    "wxyz"    // 9: wxyz
};

MatrixT9Keyboard::MatrixT9Keyboard() {
    m_state.current_mode = T9_MODE_NUMERIC;
    m_state.current_input = "";
    m_state.selected_candidate = 0;
    m_state.is_uppercase = false;
    m_state.last_key_time = 0;
    m_state.same_key_count = 0;
    
    initKeyMappings();
    initT9Dictionary();
}

void MatrixT9Keyboard::init() {
    // 初始化完成，构造函数已执行
}

void MatrixT9Keyboard::initKeyMappings() {
    // 初始化矩阵键盘到T9功能的映射
    // 行1
    m_key_mappings[MATRIX_KEY_ROW1_COL1] = {
        MATRIX_KEY_ROW1_COL1,
        0x31,  // PC键码: 1
        "1",
        "",
        "数字1"
    };
    
    m_key_mappings[MATRIX_KEY_ROW1_COL2] = {
        MATRIX_KEY_ROW1_COL2,
        0x32,  // PC键码: 2
        "2 ABC",
        "abc",
        "数字2 / ABC"
    };
    
    m_key_mappings[MATRIX_KEY_ROW1_COL3] = {
        MATRIX_KEY_ROW1_COL3,
        0x33,  // PC键码: 3
        "3 DEF",
        "def",
        "数字3 / DEF"
    };
    
    m_key_mappings[MATRIX_KEY_ROW1_COL4] = {
        MATRIX_KEY_ROW1_COL4,
        0x1B,  // PC键码: ESC
        "ESC",
        "",
        "退出/取消"
    };
    
    // 行2
    m_key_mappings[MATRIX_KEY_ROW2_COL1] = {
        MATRIX_KEY_ROW2_COL1,
        0x34,  // PC键码: 4
        "4 GHI",
        "ghi",
        "数字4 / GHI"
    };
    
    m_key_mappings[MATRIX_KEY_ROW2_COL2] = {
        MATRIX_KEY_ROW2_COL2,
        0x35,  // PC键码: 5
        "5 JKL",
        "jkl",
        "数字5 / JKL"
    };
    
    m_key_mappings[MATRIX_KEY_ROW2_COL3] = {
        MATRIX_KEY_ROW2_COL3,
        0x36,  // PC键码: 6
        "6 MNO",
        "mno",
        "数字6 / MNO"
    };
    
    m_key_mappings[MATRIX_KEY_ROW2_COL4] = {
        MATRIX_KEY_ROW2_COL4,
        0x08,  // PC键码: Backspace
        "MENU ←",
        "",
        "菜单/退格"
    };
    
    // 行3
    m_key_mappings[MATRIX_KEY_ROW3_COL1] = {
        MATRIX_KEY_ROW3_COL1,
        0x37,  // PC键码: 7
        "7 PQRS",
        "pqrs",
        "数字7 / PQRS"
    };
    
    m_key_mappings[MATRIX_KEY_ROW3_COL2] = {
        MATRIX_KEY_ROW3_COL2,
        0x38,  // PC键码: 8
        "8 TUV",
        "tuv",
        "数字8 / TUV"
    };
    
    m_key_mappings[MATRIX_KEY_ROW3_COL3] = {
        MATRIX_KEY_ROW3_COL3,
        0x39,  // PC键码: 9
        "9 WXYZ",
        "wxyz",
        "数字9 / WXYZ"
    };
    
    m_key_mappings[MATRIX_KEY_ROW3_COL4] = {
        MATRIX_KEY_ROW3_COL4,
        0x26,  // PC键码: Up Arrow
        "▲",
        "",
        "上方向键"
    };
    
    // 行4
    m_key_mappings[MATRIX_KEY_ROW4_COL1] = {
        MATRIX_KEY_ROW4_COL1,
        0x23,  // PC键码: #
        "● #",
        "",
        "切换输入法"
    };
    
    m_key_mappings[MATRIX_KEY_ROW4_COL2] = {
        MATRIX_KEY_ROW4_COL2,
        0x30,  // PC键码: 0
        "0 _",
        " ",
        "数字0 / 空格"
    };
    
    m_key_mappings[MATRIX_KEY_ROW4_COL3] = {
        MATRIX_KEY_ROW4_COL3,
        0x0D,  // PC键码: Enter
        "OK",
        "",
        "确认/回车"
    };
    
    m_key_mappings[MATRIX_KEY_ROW4_COL4] = {
        MATRIX_KEY_ROW4_COL4,
        0x28,  // PC键码: Down Arrow
        "▼",
        "",
        "下方向键"
    };
}

void MatrixT9Keyboard::initT9Dictionary() {
    // 初始化简单的T9英文词典
    m_english_dict["2"] = {"a", "b", "c"};
    m_english_dict["22"] = {"ba", "ca", "ab"};
    m_english_dict["3"] = {"d", "e", "f"};
    m_english_dict["33"] = {"ed", "fe", "de"};
    m_english_dict["4"] = {"g", "h", "i"};
    m_english_dict["5"] = {"j", "k", "l"};
    m_english_dict["6"] = {"m", "n", "o"};
    m_english_dict["7"] = {"p", "q", "r", "s"};
    m_english_dict["8"] = {"t", "u", "v"};
    m_english_dict["9"] = {"w", "x", "y", "z"};
    
    // 初始化简单的拼音词典
    m_pinyin_dict["hao"] = {"好", "号", "豪"};
    m_pinyin_dict["ni"] = {"你", "尼", "泥"};
    m_pinyin_dict["hao2"] = {"好", "号"};
    m_pinyin_dict["ni3"] = {"你", "拟"};
    m_pinyin_dict["zhong"] = {"中", "重", "钟"};
    m_pinyin_dict["wen"] = {"文", "问", "温"};
    m_pinyin_dict["zhong1"] = {"中", "钟"};
    m_pinyin_dict["wen2"] = {"文", "闻"};
}

bool MatrixT9Keyboard::handleMatrixKeyPress(MatrixKey key) {
    if (key < 0 || key >= MATRIX_KEY_COUNT) {
        return false;
    }
    
    // 获取当前时间戳
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // 检查是否是相同按键连续按下
    if (m_state.last_key_time > 0 && (now - m_state.last_key_time) < 1000) {
        // 1秒内连续按下相同按键
        m_state.same_key_count++;
    } else {
        m_state.same_key_count = 1;
    }
    
    m_state.last_key_time = now;
    
    // 根据按键类型处理
    const MatrixKeyMapping& mapping = m_key_mappings[key];
    
    // 判断是否是数字键（有字母映射的键）
    if (!mapping.letters.empty()) {
        // 数字键处理
        uint8_t digit = 0;
        switch (key) {
            case MATRIX_KEY_ROW1_COL1: digit = 1; break;
            case MATRIX_KEY_ROW1_COL2: digit = 2; break;
            case MATRIX_KEY_ROW1_COL3: digit = 3; break;
            case MATRIX_KEY_ROW2_COL1: digit = 4; break;
            case MATRIX_KEY_ROW2_COL2: digit = 5; break;
            case MATRIX_KEY_ROW2_COL3: digit = 6; break;
            case MATRIX_KEY_ROW3_COL1: digit = 7; break;
            case MATRIX_KEY_ROW3_COL2: digit = 8; break;
            case MATRIX_KEY_ROW3_COL3: digit = 9; break;
            case MATRIX_KEY_ROW4_COL2: digit = 0; break;
            default: digit = 0; break;
        }
        
        handleDigitInput(digit);
        return true;
    } else {
        // 功能键处理
        handleFunctionKey(key);
        return true;
    }
}

void MatrixT9Keyboard::handleMatrixKeyRelease(MatrixKey key) {
    // 按键释放处理，当前版本不需要特殊处理
    (void)key;
}

void MatrixT9Keyboard::handleDigitInput(uint8_t digit) {
    // 将数字添加到当前输入序列
    char digit_char = '0' + digit;
    m_state.current_input += digit_char;
    
    // 根据当前模式处理输入
    switch (m_state.current_mode) {
        case T9_MODE_NUMERIC:
            // 数字模式：直接输出数字
            outputText(std::string(1, digit_char));
            m_state.current_input.clear();
            break;
            
        case T9_MODE_ENGLISH:
            // 英文模式：T9输入法处理
            processT9Input();
            break;
            
        case T9_MODE_CHINESE:
            // 中文模式：拼音输入法处理
            processT9Input();
            break;
            
        case T9_MODE_SYMBOL:
            // 符号模式：数字直接输出
            outputText(std::string(1, digit_char));
            m_state.current_input.clear();
            break;
    }
}

void MatrixT9Keyboard::handleFunctionKey(MatrixKey key) {
    switch (key) {
        case MATRIX_KEY_ROW1_COL4:  // ESC
            // 清空输入
            clearInput();
            // 可以发送ESC键事件
            if (m_text_callback) {
                m_text_callback("[ESC]");
            }
            break;
            
        case MATRIX_KEY_ROW2_COL4:  // MENU ← (退格)
            if (!m_state.current_input.empty()) {
                m_state.current_input.pop_back();
                updateCandidates();
            } else {
                // 发送退格键事件
                if (m_text_callback) {
                    m_text_callback("[BACKSPACE]");
                }
            }
            break;
            
        case MATRIX_KEY_ROW3_COL4:  // ▲ (上方向键)
            // 上方向键：选择上一个候选词
            if (!m_state.candidates.empty()) {
                if (m_state.selected_candidate > 0) {
                    m_state.selected_candidate--;
                } else {
                    m_state.selected_candidate = m_state.candidates.size() - 1;
                }
            }
            break;
            
        case MATRIX_KEY_ROW4_COL1:  // ● # (切换输入法)
            // 循环切换输入模式
            {
                T9InputMode next_mode = static_cast<T9InputMode>(
                    (static_cast<int>(m_state.current_mode) + 1) % 4
                );
                switchInputMode(next_mode);
            }
            break;
            
        case MATRIX_KEY_ROW4_COL3:  // OK (回车)
            // 如果有选中的候选词，输出它
            if (!m_state.candidates.empty() && m_state.selected_candidate < m_state.candidates.size()) {
                std::string selected = m_state.candidates[m_state.selected_candidate];
                outputText(selected);
                m_state.current_input.clear();
                m_state.candidates.clear();
            } else if (!m_state.current_input.empty()) {
                // 直接输出当前输入
                outputText(m_state.current_input);
                m_state.current_input.clear();
            } else {
                // 发送回车键事件
                if (m_text_callback) {
                    m_text_callback("[ENTER]");
                }
            }
            break;
            
        case MATRIX_KEY_ROW4_COL4:  // ▼ (下方向键)
            // 下方向键：选择下一个候选词
            if (!m_state.candidates.empty()) {
                m_state.selected_candidate = (m_state.selected_candidate + 1) % m_state.candidates.size();
            }
            break;
            
        default:
            // 其他功能键暂时不处理
            break;
    }
}

void MatrixT9Keyboard::processT9Input() {
    // 更新候选词
    updateCandidates();
    
    // 如果只有一个候选词，可以自动选择
    if (m_state.candidates.size() == 1 && m_state.current_input.length() >= 3) {
        // 输入较长且只有一个候选词时，可以自动选择
        // 这里不自动选择，让用户确认
    }
}

void MatrixT9Keyboard::updateCandidates() {
    m_state.candidates.clear();
    m_state.selected_candidate = 0;
    
    if (m_state.current_input.empty()) {
        return;
    }
    
    // 根据当前模式查找候选词
    switch (m_state.current_mode) {
        case T9_MODE_ENGLISH:
            // 英文模式：查找T9英文单词
            if (m_english_dict.find(m_state.current_input) != m_english_dict.end()) {
                m_state.candidates = m_english_dict[m_state.current_input];
                
                // 处理大小写
                if (m_state.is_uppercase) {
                    for (auto& candidate : m_state.candidates) {
                        std::transform(candidate.begin(), candidate.end(), candidate.begin(), ::toupper);
                    }
                }
            } else {
                // 如果没有找到完整匹配，生成可能的字母组合
                std::string letters;
                for (char c : m_state.current_input) {
                    int digit = c - '0';
                    if (digit >= 0 && digit <= 9 && s_digit_to_letters[digit][0] != '\0') {
                        letters += s_digit_to_letters[digit];
                    }
                }
                
                if (!letters.empty()) {
                    // 取前几个字母作为候选
                    for (size_t i = 0; i < std::min(letters.size(), static_cast<size_t>(3)); i++) {
                        m_state.candidates.push_back(std::string(1, letters[i]));
                    }
                }
            }
            break;
            
        case T9_MODE_CHINESE:
            // 中文模式：查找拼音对应的汉字
            {
                std::vector<std::string> chinese_chars = findChineseCharacters(m_state.current_input);
                m_state.candidates = chinese_chars;
            }
            break;
            
        default:
            // 其他模式不需要候选词
            break;
    }
}

std::vector<std::string> MatrixT9Keyboard::findChineseCharacters(const std::string& pinyin) const {
    std::vector<std::string> result;
    
    // 在拼音字典中查找
    auto it = m_pinyin_dict.find(pinyin);
    if (it != m_pinyin_dict.end()) {
        result = it->second;
    } else {
        // 如果没有精确匹配，尝试模糊匹配（去掉声调）
        std::string pinyin_no_tone = pinyin;
        if (!pinyin.empty() && isdigit(pinyin.back())) {
            pinyin_no_tone = pinyin.substr(0, pinyin.length() - 1);
        }
        
        it = m_pinyin_dict.find(pinyin_no_tone);
        if (it != m_pinyin_dict.end()) {
            result = it->second;
        }
    }
    
    return result;
}

std::string MatrixT9Keyboard::getCurrentText() const {
    if (!m_state.candidates.empty() && m_state.selected_candidate < m_state.candidates.size()) {
        return m_state.candidates[m_state.selected_candidate];
    }
    return m_state.current_input;
}

std::vector<std::string> MatrixT9Keyboard::getCandidates() const {
    return m_state.candidates;
}

bool MatrixT9Keyboard::selectCandidate(size_t index) {
    if (index < m_state.candidates.size()) {
        m_state.selected_candidate = index;
        return true;
    }
    return false;
}

void MatrixT9Keyboard::switchInputMode(T9InputMode mode) {
    m_state.current_mode = mode;
    m_state.current_input.clear();
    m_state.candidates.clear();
    m_state.selected_candidate = 0;
    m_state.same_key_count = 0;
    
    // 重置大小写状态（英文模式时有效）
    m_state.is_uppercase = (mode == T9_MODE_ENGLISH && m_state.is_uppercase);
}

T9InputMode MatrixT9Keyboard::getCurrentMode() const {
    return m_state.current_mode;
}

void MatrixT9Keyboard::clearInput() {
    m_state.reset();
}

std::string MatrixT9Keyboard::getKeyLabel(MatrixKey key) const {
    if (key >= 0 && key < MATRIX_KEY_COUNT) {
        return m_key_mappings[key].t9_label;
    }
    return "";
}

std::string MatrixT9Keyboard::getKeyDescription(MatrixKey key) const {
    if (key >= 0 && key < MATRIX_KEY_COUNT) {
        return m_key_mappings[key].description;
    }
    return "";
}

uint32_t MatrixT9Keyboard::getPCKeycode(MatrixKey key) const {
    if (key >= 0 && key < MATRIX_KEY_COUNT) {
        return m_key_mappings[key].pc_keycode;
    }
    return 0;
}

std::string MatrixT9Keyboard::getT9LayoutDescription() {
    return R"(T9键盘布局 (4x4矩阵键盘映射):
┌───────┬───────┬───────┬───────┐
│  1    │ 2 ABC │ 3 DEF │  ESC  │
├───────┼───────┼───────┼───────┤
│ 4 GHI │ 5 JKL │ 6 MNO │ MENU ←│
├───────┼───────┼───────┼───────┤
│7 PQRS │ 8 TUV │9 WXYZ │   ▲   │
├───────┼───────┼───────┼───────┤
│  ● #  │  0 _  │  OK   │   ▼   │
└───────┴───────┴───────┴───────┘

功能说明:
• ESC: 退出/取消
• MENU ←: 菜单/退格键
• ▲/▼: 上下方向键，用于选择候选词
• ● #: 切换输入法 (数字/英文/中文/符号)
• OK: 确认/回车键
• 数字键: 长按可切换字母/候选词
)";
}

void MatrixT9Keyboard::setTextOutputCallback(std::function<void(const std::string&)> callback) {
    m_text_callback = callback;
}

std::string MatrixT9Keyboard::getLettersForDigit(uint8_t digit) const {
    if (digit <= 9) {
        return s_digit_to_letters[digit];
    }
    return "";
}

void MatrixT9Keyboard::outputText(const std::string& text) {
    if (m_text_callback && !text.empty()) {
        m_text_callback(text);
    }
}

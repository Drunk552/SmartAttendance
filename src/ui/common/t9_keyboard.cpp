/**
 * @file t9_keyboard.cpp
 * @brief T9键盘映射表与输入法逻辑实现
 */

#include "t9_keyboard.h"
#include <algorithm>
#include <cstring>
#include <ctime>

// T9数字到字母的标准映射
const std::map<uint8_t, std::vector<char>> T9Keyboard::s_digit_to_letters = {
    {1, {}},                     // 1键没有字母
    {2, {'a', 'b', 'c'}},       // 2: ABC
    {3, {'d', 'e', 'f'}},       // 3: DEF
    {4, {'g', 'h', 'i'}},       // 4: GHI
    {5, {'j', 'k', 'l'}},       // 5: JKL
    {6, {'m', 'n', 'o'}},       // 6: MNO
    {7, {'p', 'q', 'r', 's'}},  // 7: PQRS
    {8, {'t', 'u', 'v'}},       // 8: TUV
    {9, {'w', 'x', 'y', 'z'}},  // 9: WXYZ
    {0, {' ', '_'}}             // 0: 空格和下划线
};

// 数字到符号的映射
const std::map<uint8_t, std::vector<std::string>> T9Keyboard::s_digit_to_symbols = {
    {1, {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"}},
    {2, {".", ",", "?", "!", ":", ";", "'", "\""}},
    {3, {"+", "-", "=", "<", ">", "/", "\\", "|"}},
    {4, {"[", "]", "{", "}", "(", ")", "<", ">"}},
    {5, {"~", "`", "@", "#", "$", "%", "^", "&"}},
    {6, {"*", "(", ")", "-", "_", "=", "+", "["}},
    {7, {"]", "{", "}", "|", "\\", ";", ":", "'"}},
    {8, {"\"", ",", ".", "/", "<", ">", "?", "~"}},
    {9, {"`", "!", "@", "#", "$", "%", "^", "&"}},
    {0, {" ", "\t", "\n", "_", "-", "+", "=", "*"}}
};

T9Keyboard::T9Keyboard() {
    m_state.current_mode = T9InputMode::MODE_NUMERIC;
    m_state.is_uppercase = false;
    m_state.last_key_press_time = 0;
    m_state.key_repeat_count = 0;
    m_state.selected_candidate = 0;
    
    initKeyMappings();
    initT9Dictionary();
}

void T9Keyboard::init() {
    // 初始化完成
}

void T9Keyboard::initKeyMappings() {
    // 初始化T9键盘到PC键盘的映射
    // 数字键映射
    m_key_mappings[T9Key::KEY_1] = {T9Key::KEY_1, 0x31, "1", {}, nullptr};
    m_key_mappings[T9Key::KEY_2] = {T9Key::KEY_2, 0x32, "2 ABC", {'a', 'b', 'c'}, nullptr};
    m_key_mappings[T9Key::KEY_3] = {T9Key::KEY_3, 0x33, "3 DEF", {'d', 'e', 'f'}, nullptr};
    m_key_mappings[T9Key::KEY_4] = {T9Key::KEY_4, 0x34, "4 GHI", {'g', 'h', 'i'}, nullptr};
    m_key_mappings[T9Key::KEY_5] = {T9Key::KEY_5, 0x35, "5 JKL", {'j', 'k', 'l'}, nullptr};
    m_key_mappings[T9Key::KEY_6] = {T9Key::KEY_6, 0x36, "6 MNO", {'m', 'n', 'o'}, nullptr};
    m_key_mappings[T9Key::KEY_7] = {T9Key::KEY_7, 0x37, "7 PQRS", {'p', 'q', 'r', 's'}, nullptr};
    m_key_mappings[T9Key::KEY_8] = {T9Key::KEY_8, 0x38, "8 TUV", {'t', 'u', 'v'}, nullptr};
    m_key_mappings[T9Key::KEY_9] = {T9Key::KEY_9, 0x39, "9 WXYZ", {'w', 'x', 'y', 'z'}, nullptr};
    m_key_mappings[T9Key::KEY_0] = {T9Key::KEY_0, 0x30, "0 _", {' ', '_'}, nullptr};
    
    // 功能键映射
    m_key_mappings[T9Key::KEY_STAR] = {T9Key::KEY_STAR, 0x2A, "● #", {}, nullptr};
    m_key_mappings[T9Key::KEY_POUND] = {T9Key::KEY_POUND, 0x23, "#", {}, nullptr};
    m_key_mappings[T9Key::KEY_ESC] = {T9Key::KEY_ESC, 0x1B, "ESC", {}, nullptr};
    m_key_mappings[T9Key::KEY_MENU] = {T9Key::KEY_MENU, 0x0D, "MENU ←", {}, nullptr};
    m_key_mappings[T9Key::KEY_UP] = {T9Key::KEY_UP, 0x26, "▲", {}, nullptr};
    m_key_mappings[T9Key::KEY_DOWN] = {T9Key::KEY_DOWN, 0x28, "▼", {}, nullptr};
    m_key_mappings[T9Key::KEY_OK] = {T9Key::KEY_OK, 0x0D, "OK", {}, nullptr};
}

void T9Keyboard::initT9Dictionary() {
    // 初始化拼音字典（简化版，实际项目需要完整的拼音字典）
    m_pinyin_dict["a"] = {"啊", "阿", "吖"};
    m_pinyin_dict["ai"] = {"爱", "艾", "唉", "挨"};
    m_pinyin_dict["an"] = {"安", "按", "案", "暗"};
    m_pinyin_dict["ba"] = {"吧", "把", "八", "巴"};
    m_pinyin_dict["bai"] = {"白", "百", "摆", "败"};
    m_pinyin_dict["bao"] = {"包", "保", "报", "宝"};
    m_pinyin_dict["bei"] = {"被", "北", "备", "背"};
    m_pinyin_dict["ben"] = {"本", "奔", "笨", "苯"};
    m_pinyin_dict["bi"] = {"比", "必", "笔", "毕"};
    
    // 初始化英文单词字典（简化版）
    m_english_dict["2"] = {"a", "b", "c"};
    m_english_dict["22"] = {"ab", "ac", "ba", "bc", "ca", "cb"};
    m_english_dict["23"] = {"ad", "ae", "af", "bd", "be", "bf", "cd", "ce", "cf"};
    m_english_dict["3"] = {"d", "e", "f"};
    m_english_dict["33"] = {"dd", "de", "df", "ed", "ee", "ef", "fd", "fe", "ff"};
    m_english_dict["4"] = {"g", "h", "i"};
    m_english_dict["5"] = {"j", "k", "l"};
    m_english_dict["6"] = {"m", "n", "o"};
    m_english_dict["7"] = {"p", "q", "r", "s"};
    m_english_dict["8"] = {"t", "u", "v"};
    m_english_dict["9"] = {"w", "x", "y", "z"};
    
    // 添加一些常用单词
    m_english_dict["2665"] = {"book", "cool", "ammo"};
    m_english_dict["4663"] = {"home", "good", "gone"};
    m_english_dict["7378"] = {"rest", "pert", "serv"};
}

bool T9Keyboard::handleKeyPress(T9Key key) {
    uint32_t current_time = static_cast<uint32_t>(time(nullptr));
    
    // 检查是否是重复按键（用于T9的多字母选择）
    if (m_state.last_key_press_time > 0 && 
        (current_time - m_state.last_key_press_time) < 2) { // 2秒内
        m_state.key_repeat_count++;
    } else {
        m_state.key_repeat_count = 0;
    }
    
    m_state.last_key_press_time = current_time;
    
    // 根据当前模式处理按键
    switch (m_state.current_mode) {
        case T9InputMode::MODE_NUMERIC:
            handleNumericKey(key);
            break;
        case T9InputMode::MODE_ENGLISH:
        case T9InputMode::MODE_CHINESE:
            if (key >= T9Key::KEY_1 && key <= T9Key::KEY_9) {
                handleNumericKey(key);
            } else {
                handleFunctionKey(key);
            }
            break;
        case T9InputMode::MODE_SYMBOL:
            handleFunctionKey(key);
            break;
    }
    
    // 更新候选词
    updateCandidates();
    
    return true;
}

void T9Keyboard::handleKeyRelease(T9Key key) {
    // 按键释放处理（如果需要）
}

void T9Keyboard::handleNumericKey(T9Key key) {
    uint8_t digit = static_cast<uint8_t>(key);
    
    // 将数字添加到当前输入
    m_state.current_input += std::to_string(digit);
    
    // 根据当前模式处理输入
    switch (m_state.current_mode) {
        case T9InputMode::MODE_ENGLISH:
            processEnglishT9();
            break;
        case T9InputMode::MODE_CHINESE:
            processChineseT9();
            break;
        case T9InputMode::MODE_NUMERIC:
            // 数字模式直接显示数字
            if (m_state.candidates.empty()) {
                m_state.candidates.push_back(std::to_string(digit));
            }
            break;
        case T9InputMode::MODE_SYMBOL:
            // 符号模式
            if (s_digit_to_symbols.find(digit) != s_digit_to_symbols.end()) {
                const auto& symbols = s_digit_to_symbols.at(digit);
                m_state.candidates = symbols;
            }
            break;
    }
}

void T9Keyboard::handleFunctionKey(T9Key key) {
    switch (key) {
        case T9Key::KEY_ESC:
            // ESC键：清空输入或取消
            clearInput();
            break;
            
        case T9Key::KEY_UP:
            // 上箭头：选择上一个候选词
            if (!m_state.candidates.empty()) {
                if (m_state.selected_candidate > 0) {
                    m_state.selected_candidate--;
                } else {
                    m_state.selected_candidate = m_state.candidates.size() - 1;
                }
            }
            break;
            
        case T9Key::KEY_DOWN:
            // 下箭头：选择下一个候选词
            if (!m_state.candidates.empty()) {
                m_state.selected_candidate = (m_state.selected_candidate + 1) % m_state.candidates.size();
            }
            break;
            
        case T9Key::KEY_OK:
            // OK键：确认选择
            if (!m_state.candidates.empty() && m_state.selected_candidate < m_state.candidates.size()) {
                std::string selected = m_state.candidates[m_state.selected_candidate];
                // 这里可以触发文本输入事件
                clearInput();
            }
            break;
            
        case T9Key::KEY_POUND:
            // #键：切换输入模式
            {
                int current = static_cast<int>(m_state.current_mode);
                current = (current + 1) % 4; // 循环切换4种模式
                m_state.current_mode = static_cast<T9InputMode>(current);
                clearInput();
            }
            break;
            
        case T9Key::KEY_MENU:
            // MENU键：跳转到主菜单
            // 这里可以触发菜单事件
            clearInput();
            break;
            
        case T9Key::KEY_STAR:
            // *键：切换大小写（英文模式）或显示符号
            if (m_state.current_mode == T9InputMode::MODE_ENGLISH) {
                m_state.is_uppercase = !m_state.is_uppercase;
            }
            break;
            
        default:
            break;
    }
}

void T9Keyboard::processEnglishT9() {
    // 根据数字输入查找可能的英文单词
    std::string digits = m_state.current_input;
    
    if (m_english_dict.find(digits) != m_english_dict.end()) {
        m_state.candidates = m_english_dict[digits];
        
        // 如果需要大写，转换候选词
        if (m_state.is_uppercase) {
            for (auto& candidate : m_state.candidates) {
                std::transform(candidate.begin(), candidate.end(), candidate.begin(), ::toupper);
            }
        }
    } else {
        // 如果没有找到完整单词，显示可能的字母组合
        m_state.candidates.clear();
        std::string possible_word;
        
        for (char digit_char : digits) {
            uint8_t digit = digit_char - '0';
            auto letters = getLettersForDigit(digit);
            if (!letters.empty()) {
                possible_word += letters[0];
            }
        }
        
        if (!possible_word.empty()) {
            if (m_state.is_uppercase) {
                std::transform(possible_word.begin(), possible_word.end(), possible_word.begin(), ::toupper);
            }
            m_state.candidates.push_back(possible_word);
        }
    }
    
    m_state.selected_candidate = 0;
}

void T9Keyboard::processChineseT9() {
    // 根据数字输入查找可能的拼音
    std::string digits = m_state.current_input;
    std::string pinyin;
    
    // 将数字序列转换为可能的拼音（简化处理）
    // 实际项目中需要更复杂的拼音匹配算法
    for (char digit_char : digits) {
        uint8_t digit = digit_char - '0';
        auto letters = getLettersForDigit(digit);
        if (!letters.empty()) {
            pinyin += letters[0]; // 简单取第一个字母
        }
    }
    
    // 查找拼音对应的汉字
    m_state.candidates = findChineseCharacters(pinyin);
    m_state.selected_candidate = 0;
}

void T9Keyboard::updateCandidates() {
    // 根据当前输入和模式更新候选词
    if (m_state.current_input.empty()) {
        m_state.candidates.clear();
        return;
    }
    
    switch (m_state.current_mode) {
        case T9InputMode::MODE_ENGLISH:
            processEnglishT9();
            break;
        case T9InputMode::MODE_CHINESE:
            processChineseT9();
            break;
        case T9InputMode::MODE_NUMERIC:
            // 数字模式：候选词就是输入的数字
            m_state.candidates = {m_state.current_input};
            break;
        case T9InputMode::MODE_SYMBOL:
            // 符号模式：显示符号候选
            if (!m_state.current_input.empty()) {
                char last_digit_char = m_state.current_input.back();
                uint8_t digit = last_digit_char - '0';
                if (s_digit_to_symbols.find(digit) != s_digit_to_symbols.end()) {
                    m_state.candidates = s_digit_to_symbols.at(digit);
                }
            }
            break;
    }
}

std::vector<char> T9Keyboard::getLettersForDigit(uint8_t digit) const {
    if (s_digit_to_letters.find(digit) != s_digit_to_letters.end()) {
        return s_digit_to_letters.at(digit);
    }
    return {};
}

std::vector<std::string> T9Keyboard::findChineseCharacters(const std::string& pinyin) const {
    std::vector<std::string> result;
    
    // 精确匹配
    if (m_pinyin_dict.find(pinyin) != m_pinyin_dict.end()) {
        result = m_pinyin_dict.at(pinyin);
    } else {
        // 模糊匹配：查找以该拼音开头的所有汉字
        for (const auto& entry : m_pinyin_dict) {
            if (entry.first.find(pinyin) == 0) { // 以pinyin开头
                result.insert(result.end(), entry.second.begin(), entry.second.end());
            }
        }
    }
    
    return result;
}

std::string T9Keyboard::getCurrentText() const {
    if (!m_state.candidates.empty() && m_state.selected_candidate < m_state.candidates.size()) {
        return m_state.candidates[m_state.selected_candidate];
    }
    return m_state.current_input;
}

std::vector<std::string> T9Keyboard::getCandidates() const {
    return m_state.candidates;
}

bool T9Keyboard::selectCandidate(size_t index) {
    if (index < m_state.candidates.size()) {
        m_state.selected_candidate = index;
        return true;
    }
    return false;
}

void T9Keyboard::switchInputMode(T9InputMode mode) {
    m_state.current_mode = mode;
    clearInput();
}

T9InputMode T9Keyboard::getCurrentMode() const {
    return m_state.current_mode;
}

void T9Keyboard::clearInput() {
    m_state.reset();
}

uint32_t T9Keyboard::getPCKeycode(T9Key key) const {
    if (m_key_mappings.find(key) != m_key_mappings.end()) {
        return m_key_mappings.at(key).pc_keycode;
    }
    return 0;
}

std::string T9Keyboard::getKeyLabel(T9Key key) const {
    if (m_key_mappings.find(key) != m_key_mappings.end()) {
        return m_key_mappings.at(key).label;
    }
    return "";
}
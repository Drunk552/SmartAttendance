/**
 * @file t9_keymap.cpp
 * @brief T9键盘映射表实现
 */

#include "t9_keymap.h"
#include <algorithm>

T9KeyMap::T9KeyMap() {
    initDefaultKeymap();
}

void T9KeyMap::init() {
    // 已经初始化
}

void T9KeyMap::initDefaultKeymap() {
    // 根据用户提供的T9键盘布局初始化映射
    
    // 数字键
    m_keymap[T9_KEY_1] = {T9_KEY_1, 0x31, "1", "", "数字1"};
    m_keymap[T9_KEY_2] = {T9_KEY_2, 0x32, "2 ABC", "abc", "数字2，对应字母ABC"};
    m_keymap[T9_KEY_3] = {T9_KEY_3, 0x33, "3 DEF", "def", "数字3，对应字母DEF"};
    m_keymap[T9_KEY_4] = {T9_KEY_4, 0x34, "4 GHI", "ghi", "数字4，对应字母GHI"};
    m_keymap[T9_KEY_5] = {T9_KEY_5, 0x35, "5 JKL", "jkl", "数字5，对应字母JKL"};
    m_keymap[T9_KEY_6] = {T9_KEY_6, 0x36, "6 MNO", "mno", "数字6，对应字母MNO"};
    m_keymap[T9_KEY_7] = {T9_KEY_7, 0x37, "7 PQRS", "pqrs", "数字7，对应字母PQRS"};
    m_keymap[T9_KEY_8] = {T9_KEY_8, 0x38, "8 TUV", "tuv", "数字8，对应字母TUV"};
    m_keymap[T9_KEY_9] = {T9_KEY_9, 0x39, "9 WXYZ", "wxyz", "数字9，对应字母WXYZ"};
    m_keymap[T9_KEY_0] = {T9_KEY_0, 0x30, "0 _", " _", "数字0，空格和下划线"};
    
    // 功能键
    m_keymap[T9_KEY_STAR] = {T9_KEY_STAR, 0x2A, "● #", "", "星号键，切换输入法"};
    m_keymap[T9_KEY_POUND] = {T9_KEY_POUND, 0x23, "#", "", "井号键，特殊功能"};
    m_keymap[T9_KEY_ESC] = {T9_KEY_ESC, 0x1B, "ESC", "", "退出键"};
    m_keymap[T9_KEY_MENU] = {T9_KEY_MENU, 0x0D, "MENU ←", "", "菜单键，返回主菜单"};
    m_keymap[T9_KEY_UP] = {T9_KEY_UP, 0x26, "▲", "", "上方向键"};
    m_keymap[T9_KEY_DOWN] = {T9_KEY_DOWN, 0x28, "▼", "", "下方向键"};
    m_keymap[T9_KEY_OK] = {T9_KEY_OK, 0x0D, "OK", "", "确认键（回车）"};
}

uint32_t T9KeyMap::getPCKeycode(T9KeyCode t9_key) const {
    auto it = m_keymap.find(t9_key);
    if (it != m_keymap.end()) {
        return it->second.pc_keycode;
    }
    return 0;
}

std::string T9KeyMap::getLabel(T9KeyCode t9_key) const {
    auto it = m_keymap.find(t9_key);
    if (it != m_keymap.end()) {
        return it->second.label;
    }
    return "";
}

std::string T9KeyMap::getLetters(T9KeyCode t9_key) const {
    auto it = m_keymap.find(t9_key);
    if (it != m_keymap.end()) {
        return it->second.letters;
    }
    return "";
}

std::string T9KeyMap::getDescription(T9KeyCode t9_key) const {
    auto it = m_keymap.find(t9_key);
    if (it != m_keymap.end()) {
        return it->second.description;
    }
    return "";
}

T9KeyCode T9KeyMap::findT9KeyByPCKeycode(uint32_t pc_keycode) const {
    for (const auto& pair : m_keymap) {
        if (pair.second.pc_keycode == pc_keycode) {
            return pair.first;
        }
    }
    return T9_KEY_0; // 默认返回0键
}

std::vector<T9KeyCode> T9KeyMap::getAllT9Keys() const {
    std::vector<T9KeyCode> keys;
    for (const auto& pair : m_keymap) {
        keys.push_back(pair.first);
    }
    return keys;
}

std::string T9KeyMap::getLettersForDigit(uint8_t digit) {
    // 标准T9数字到字母的映射
    switch (digit) {
        case 1: return "";
        case 2: return "abc";
        case 3: return "def";
        case 4: return "ghi";
        case 5: return "jkl";
        case 6: return "mno";
        case 7: return "pqrs";
        case 8: return "tuv";
        case 9: return "wxyz";
        case 0: return " _";
        default: return "";
    }
}

std::string T9KeyMap::getLayoutDescription() {
    return R"(T9键盘布局 (4x4):
┌───────┬───────┬───────┬───────┐
│   1   │ 2 ABC │ 3 DEF │  ESC  │
├───────┼───────┼───────┼───────┤
│ 4 GHI │ 5 JKL │ 6 MNO │MENU ← │
├───────┼───────┼───────┼───────┤
│7 PQRS │ 8 TUV │9 WXYZ │   ▲   │
├───────┼───────┼───────┼───────┤
│  ● #  │  0 _  │   OK  │   ▼   │
└───────┴───────┴───────┴───────┘

功能说明:
1. ESC键: 对应电脑键盘ESC，取消或退出
2. ↑↓键: 对应电脑键盘上下箭头，导航选择
3. OK键: 对应电脑键盘回车键，确认选择
4. #键: 切换输入法（数字/英文/中文/符号）
5. MENU键: 跳转到主菜单
6. *键: 特殊功能键
)";
}
/**
 * @file t9_keyboard.h
 * @brief T9键盘映射表与输入法逻辑
 * @details 为智能考勤系统设计的T9键盘映射，支持中文、英文和符号输入
 */

#ifndef T9_KEYBOARD_H
#define T9_KEYBOARD_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>

// T9键盘模式
enum class T9InputMode {
    MODE_NUMERIC = 0,    // 数字模式
    MODE_ENGLISH = 1,    // 英文模式
    MODE_CHINESE = 2,    // 中文模式
    MODE_SYMBOL = 3      // 符号模式
};

// T9键盘按键定义
enum class T9Key {
    KEY_1 = 1,
    KEY_2 = 2,
    KEY_3 = 3,
    KEY_4 = 4,
    KEY_5 = 5,
    KEY_6 = 6,
    KEY_7 = 7,
    KEY_8 = 8,
    KEY_9 = 9,
    KEY_0 = 0,
    KEY_STAR = 10,      // * 键
    KEY_POUND = 11,     // # 键
    KEY_ESC = 12,       // ESC键
    KEY_MENU = 13,      // MENU键
    KEY_UP = 14,        // 上箭头
    KEY_DOWN = 15,      // 下箭头
    KEY_OK = 16         // OK键
};

// T9键盘映射表
struct T9KeyMapping {
    T9Key key;                     // T9键盘按键
    uint32_t pc_keycode;           // 对应的PC键盘键码
    std::string label;             // 显示标签
    std::vector<char> letters;     // 对应的字母（用于T9输入）
    std::function<void()> action;  // 自定义动作（可选）
};

// T9输入法状态
struct T9InputState {
    T9InputMode current_mode;
    std::string current_input;
    std::vector<std::string> candidates;
    size_t selected_candidate;
    bool is_uppercase;
    uint32_t last_key_press_time;
    uint8_t key_repeat_count;
    
    // 重置状态
    void reset() {
        current_input.clear();
        candidates.clear();
        selected_candidate = 0;
        key_repeat_count = 0;
    }
};

/**
 * @class T9Keyboard
 * @brief T9键盘逻辑处理类
 */
class T9Keyboard {
public:
    T9Keyboard();
    ~T9Keyboard() = default;
    
    // 初始化T9键盘
    void init();
    
    // 处理按键按下
    bool handleKeyPress(T9Key key);
    
    // 处理按键释放
    void handleKeyRelease(T9Key key);
    
    // 获取当前输入文本
    std::string getCurrentText() const;
    
    // 获取候选词列表
    std::vector<std::string> getCandidates() const;
    
    // 选择候选词
    bool selectCandidate(size_t index);
    
    // 切换输入模式
    void switchInputMode(T9InputMode mode);
    
    // 获取当前输入模式
    T9InputMode getCurrentMode() const;
    
    // 清空输入
    void clearInput();
    
    // 获取PC键盘映射
    uint32_t getPCKeycode(T9Key key) const;
    
    // 获取按键标签
    std::string getKeyLabel(T9Key key) const;
    
private:
    // 初始化按键映射表
    void initKeyMappings();
    
    // 初始化T9字典
    void initT9Dictionary();
    
    // 处理数字键输入
    void handleNumericKey(T9Key key);
    
    // 处理功能键输入
    void handleFunctionKey(T9Key key);
    
    // 更新候选词
    void updateCandidates();
    
    // 英文T9输入处理
    void processEnglishT9();
    
    // 中文T9输入处理
    void processChineseT9();
    
    // 获取数字键对应的字母
    std::vector<char> getLettersForDigit(uint8_t digit) const;
    
    // 查找拼音对应的汉字
    std::vector<std::string> findChineseCharacters(const std::string& pinyin) const;
    
private:
    T9InputState m_state;
    std::map<T9Key, T9KeyMapping> m_key_mappings;
    std::map<std::string, std::vector<std::string>> m_pinyin_dict;  // 拼音字典
    std::map<std::string, std::vector<std::string>> m_english_dict; // 英文单词字典
    
    // T9数字到字母的映射（标准T9布局）
    static const std::map<uint8_t, std::vector<char>> s_digit_to_letters;
    
    // 常用符号映射
    static const std::map<uint8_t, std::vector<std::string>> s_digit_to_symbols;
};

#endif // T9_KEYBOARD_H
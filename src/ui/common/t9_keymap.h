/**
 * @file t9_keymap.h
 * @brief T9键盘到PC键盘的映射表
 * @details 为智能考勤系统设计的T9键盘映射，支持硬件T9键盘和软件模拟
 */

#ifndef T9_KEYMAP_H
#define T9_KEYMAP_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// T9键盘按键定义（基于用户提供的4x4布局）
enum T9KeyCode {
    T9_KEY_1 = 0x01,
    T9_KEY_2 = 0x02,
    T9_KEY_3 = 0x03,
    T9_KEY_4 = 0x04,
    T9_KEY_5 = 0x05,
    T9_KEY_6 = 0x06,
    T9_KEY_7 = 0x07,
    T9_KEY_8 = 0x08,
    T9_KEY_9 = 0x09,
    T9_KEY_0 = 0x0A,
    T9_KEY_STAR = 0x0B,     // * 键
    T9_KEY_POUND = 0x0C,    // # 键
    T9_KEY_ESC = 0x0D,
    T9_KEY_MENU = 0x0E,
    T9_KEY_UP = 0x0F,
    T9_KEY_DOWN = 0x10,
    T9_KEY_OK = 0x11
};

// T9键盘映射项
struct T9KeyMapItem {
    T9KeyCode t9_key;           // T9键盘键码
    uint32_t pc_keycode;        // 对应的PC键盘键码
    std::string label;          // 显示标签
    std::string letters;        // 对应的字母（用于T9输入）
    std::string description;    // 功能描述
};

/**
 * @brief T9键盘映射表类
 * @details 提供T9键盘到PC键盘的映射，以及T9输入法支持
 */
class T9KeyMap {
public:
    T9KeyMap();
    ~T9KeyMap() = default;
    
    // 初始化映射表
    void init();
    
    // 获取PC键盘键码
    uint32_t getPCKeycode(T9KeyCode t9_key) const;
    
    // 获取按键标签
    std::string getLabel(T9KeyCode t9_key) const;
    
    // 获取按键对应的字母
    std::string getLetters(T9KeyCode t9_key) const;
    
    // 获取功能描述
    std::string getDescription(T9KeyCode t9_key) const;
    
    // 根据PC键码查找T9键码（反向查找）
    T9KeyCode findT9KeyByPCKeycode(uint32_t pc_keycode) const;
    
    // 获取所有T9按键列表
    std::vector<T9KeyCode> getAllT9Keys() const;
    
    // T9数字到字母的映射（标准T9布局）
    static std::string getLettersForDigit(uint8_t digit);
    
    // 获取T9键盘布局描述
    static std::string getLayoutDescription();
    
private:
    std::map<T9KeyCode, T9KeyMapItem> m_keymap;
    
    // 初始化默认映射
    void initDefaultKeymap();
};

#endif // T9_KEYMAP_H
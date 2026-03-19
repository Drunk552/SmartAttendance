/**
 * @file matrix_t9_keyboard.h
 * @brief 矩阵键盘T9映射与输入法逻辑
 * @details 为智能考勤系统设计的矩阵键盘T9功能，将4x4矩阵键盘映射为T9键盘
 */

#ifndef MATRIX_T9_KEYBOARD_H
#define MATRIX_T9_KEYBOARD_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <map>

// 矩阵键盘按键定义（4x4布局）
enum MatrixKey {
    MATRIX_KEY_ROW1_COL1 = 0,  // 位置: (1,1) - 对应 T9键: 1
    MATRIX_KEY_ROW1_COL2,      // 位置: (1,2) - 对应 T9键: 2 ABC
    MATRIX_KEY_ROW1_COL3,      // 位置: (1,3) - 对应 T9键: 3 DEF
    MATRIX_KEY_ROW1_COL4,      // 位置: (1,4) - 对应 T9键: ESC
    
    MATRIX_KEY_ROW2_COL1,      // 位置: (2,1) - 对应 T9键: 4 GHI
    MATRIX_KEY_ROW2_COL2,      // 位置: (2,2) - 对应 T9键: 5 JKL
    MATRIX_KEY_ROW2_COL3,      // 位置: (2,3) - 对应 T9键: 6 MNO
    MATRIX_KEY_ROW2_COL4,      // 位置: (2,4) - 对应 T9键: MENU ←
    
    MATRIX_KEY_ROW3_COL1,      // 位置: (3,1) - 对应 T9键: 7 PQRS
    MATRIX_KEY_ROW3_COL2,      // 位置: (3,2) - 对应 T9键: 8 TUV
    MATRIX_KEY_ROW3_COL3,      // 位置: (3,3) - 对应 T9键: 9 WXYZ
    MATRIX_KEY_ROW3_COL4,      // 位置: (3,4) - 对应 T9键: ▲
    
    MATRIX_KEY_ROW4_COL1,      // 位置: (4,1) - 对应 T9键: ● #
    MATRIX_KEY_ROW4_COL2,      // 位置: (4,2) - 对应 T9键: 0 _
    MATRIX_KEY_ROW4_COL3,      // 位置: (4,3) - 对应 T9键: OK
    MATRIX_KEY_ROW4_COL4,      // 位置: (4,4) - 对应 T9键: ▼
    
    MATRIX_KEY_COUNT = 16
};

// T9输入模式
enum T9InputMode {
    T9_MODE_NUMERIC = 0,    // 数字模式
    T9_MODE_ENGLISH = 1,    // 英文模式
    T9_MODE_CHINESE = 2,    // 中文模式
    T9_MODE_SYMBOL = 3      // 符号模式
};

// 矩阵键盘到T9功能的映射项
struct MatrixKeyMapping {
    MatrixKey matrix_key;          // 矩阵键盘位置
    uint32_t pc_keycode;           // 对应的PC键盘键码（用于模拟）
    std::string t9_label;          // T9功能标签
    std::string letters;           // 对应的字母（用于T9输入）
    std::string description;       // 功能描述
};

// T9输入状态
struct T9InputState {
    T9InputMode current_mode;
    std::string current_input;     // 当前输入的数字序列
    std::vector<std::string> candidates; // 候选词列表
    size_t selected_candidate;
    bool is_uppercase;
    uint32_t last_key_time;
    uint8_t same_key_count;        // 相同按键连续按下次数（用于多字母选择）
    
    void reset() {
        current_input.clear();
        candidates.clear();
        selected_candidate = 0;
        same_key_count = 0;
    }
};

/**
 * @class MatrixT9Keyboard
 * @brief 矩阵键盘T9功能处理类
 * @details 将4x4矩阵键盘映射为T9键盘，提供T9输入法功能
 */
class MatrixT9Keyboard {
public:
    MatrixT9Keyboard();
    ~MatrixT9Keyboard() = default;
    
    // 初始化矩阵键盘映射
    void init();
    
    // 处理矩阵键盘按键事件
    bool handleMatrixKeyPress(MatrixKey key);
    void handleMatrixKeyRelease(MatrixKey key);
    
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
    
    // 获取矩阵键盘映射信息
    std::string getKeyLabel(MatrixKey key) const;
    std::string getKeyDescription(MatrixKey key) const;
    uint32_t getPCKeycode(MatrixKey key) const;
    
    // 获取T9键盘布局描述
    static std::string getT9LayoutDescription();
    
    // 设置文本输出回调（当有文本输入完成时调用）
    void setTextOutputCallback(std::function<void(const std::string&)> callback);
    
private:
    // 初始化默认映射
    void initKeyMappings();
    
    // 初始化T9字典
    void initT9Dictionary();
    
    // 处理数字键输入
    void handleDigitInput(uint8_t digit);
    
    // 处理功能键输入
    void handleFunctionKey(MatrixKey key);
    
    // 更新候选词
    void updateCandidates();
    
    // T9输入处理
    void processT9Input();
    
    // 获取数字对应的字母
    std::string getLettersForDigit(uint8_t digit) const;
    
    // 查找拼音对应的汉字
    std::vector<std::string> findChineseCharacters(const std::string& pinyin) const;
    
    // 输出文本
    void outputText(const std::string& text);
    
private:
    T9InputState m_state;
    MatrixKeyMapping m_key_mappings[MATRIX_KEY_COUNT];
    std::function<void(const std::string&)> m_text_callback;
    
    // T9字典
    std::map<std::string, std::vector<std::string>> m_pinyin_dict;
    std::map<std::string, std::vector<std::string>> m_english_dict;
    
    // 标准T9数字到字母映射
    static const char* s_digit_to_letters[10];
};

#endif // MATRIX_T9_KEYBOARD_H
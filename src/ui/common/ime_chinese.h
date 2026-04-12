#ifndef IME_CHINESE_H
#define IME_CHINESE_H

#include <vector>
#include <string>

/**
 * @brief 中文 T9 拼音输入法词典模块
 *
 * 工作原理：
 *   - 9键 T9 映射：按键序列 → 所有可能的拼音组合
 *   - 拼音词典：拼音 → 常用汉字候选列表（静态内嵌数组，~80KB）
 *
 * T9 数字键 → 拼音字母映射（标准 T9）:
 *   2=abc  3=def  4=ghi  5=jkl  6=mno  7=pqrs  8=tuv  9=wxyz
 *
 * 使用方式：
 *   1. 用户每按一个数字键，调用 ime_cn_push_digit(d) 追加一位
 *   2. 调用 ime_cn_get_candidates() 获取当前拼音前缀匹配到的汉字列表
 *   3. 用户按 OK 确认选中，调用 ime_cn_reset() 清空输入缓冲区
 *   4. 切换模式或取消时也调用 ime_cn_reset()
 */

/** 当前拼音缓冲区最大长度（拼音最长不超过6个字母，如 "zhuang"） */
#define IME_CN_MAX_PINYIN_LEN 6

/** 单次最多返回的候选汉字数 */
#define IME_CN_MAX_CANDIDATES 10

/**
 * @brief 向当前拼音缓冲区追加一个数字键（0-9）
 *        超出 IME_CN_MAX_PINYIN_LEN 后忽略
 * @param digit 0~9
 */
void ime_cn_push_digit(int digit);

/**
 * @brief 删除拼音缓冲区最后一个字母（对应 BACKSPACE 操作）
 */
void ime_cn_pop();

/**
 * @brief 重置拼音缓冲区（切换模式、确认输入后调用）
 */
void ime_cn_reset();

/**
 * @brief 获取当前拼音缓冲区对应的所有候选汉字
 * @return 候选汉字的 UTF-8 字符串列表（每项为一个汉字）
 *         缓冲区为空时返回空列表
 */
std::vector<std::string> ime_cn_get_candidates();

/**
 * @brief 获取当前正在拼写的拼音字母串（用于显示在候选条上方）
 *        例如: 按下 2-2-3 可能返回 "aad"/"aae"/"aaf"/"abd"...
 *        这里返回最短匹配前缀或当前缓冲中所有可能的第一个组合
 * @return 当前拼音缓冲的可读表示，格式 "bXX" (b 表示缓冲)
 */
std::string ime_cn_get_pinyin_display();

#endif // IME_CHINESE_H

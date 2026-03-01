#include "auth_service.h"
#include "../data/db_storage.h" // 引用数据层
#include <iostream>
#include <cstring>

// 引入考勤记录相关的业务逻辑
#include "attendance_rule.h" 

// ==========================================
// 密码验证实现
// ==========================================
AuthResult AuthService::verifyPassword(int user_id, const std::string& input_password) {

    // 1. 从数据库获取用户信息
    auto user_opt = db_get_user_info(user_id);

    // 2. 检查用户是否存在 (如果盒子是空的，说明没这个人)
    if (!user_opt.has_value()) {
        return AuthResult::USER_NOT_FOUND;
    }
    
    // 3. 拆盒取出真实数据 (后面的 user.password 验证逻辑完全不用动！)
    UserData user = user_opt.value();

    // 4. 检查用户是否设置了密码
    // 手册P13提示：密码长度1-6位
    if (user.password.empty()) {
        return AuthResult::NO_FEATURE_DATA; // 该用户未录入密码
    }

    // 5. 比对密码
    // 注意：实际生产建议比对哈希值，此处演示直接比对字符串
    if (user.password == input_password) {
        // [可选] 验证成功后，自动记录考勤
        // db_add_attendance(user.id, ...);
        return AuthResult::SUCCESS;
    } else {
        return AuthResult::WRONG_PASSWORD;
    }
}

// ==========================================
// 指纹验证实现
// ==========================================
AuthResult AuthService::verifyFingerprint(int user_id, const std::vector<uint8_t>& captured_fp_data) {
    // 1. 从数据库获取用户信息
    auto user_opt = db_get_user_info(user_id);

    // 2. 检查用户是否存在
    if (!user_opt.has_value()) {
        return AuthResult::USER_NOT_FOUND;
    }
    
    // 3. 拆盒取出真实数据 (后面的 user.fingerprint_feature 逻辑不用动)
    UserData user = user_opt.value();

    // 4. 检查数据库中是否有该用户的指纹数据
    // 注意：我们在 db_storage.cpp 中添加了 fingerprint_feature 读取逻辑
    if (user.fingerprint_feature.empty()) {
        return AuthResult::NO_FEATURE_DATA; // 用户未录入指纹
    }

    // 5. 执行指纹比对算法
    int score = matchFingerprintTemplate(user.fingerprint_feature, captured_fp_data);

    // 6. 判断得分是否通过 (假设阈值为 80 分)
    if (score >= 80) {
        // [可选] 验证成功后，自动记录考勤
        // db_add_attendance(user.id, ...);
        return AuthResult::SUCCESS;
    } else {
        return AuthResult::WRONG_FINGERPRINT;
    }
}

// ==========================================
// [占位符] 指纹算法模拟
// ==========================================
int AuthService::matchFingerprintTemplate(const std::vector<uint8_t>& stored, const std::vector<uint8_t>& captured) {
    // ---------------------------------------------------------
    // ⚠️ 警告：这是伪代码！请替换为真实的指纹SDK调用 ⚠️
    // ---------------------------------------------------------
    // 示例：调用硬件厂商的算法库
    // int score = FingerprintSDK_Match(stored.data(), stored.size(), captured.data(), captured.size());
    // return score;

    // 模拟逻辑：如果数据长度完全一样，假装是匹配的（仅供测试流程）
    if (stored.size() == captured.size() && stored.size() > 0) {
        // 简单模拟：比较前10个字节，一样就给100分
        if (memcmp(stored.data(), captured.data(), std::min((size_t)10, stored.size())) == 0) {
            return 100;
        }
    }
    return 0; // 不匹配
}
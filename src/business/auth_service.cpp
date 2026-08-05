#include "auth_service.h"
#include <iostream>
#include <cstring>

AuthService::AuthService(
    smart_attendance::storage::IAuthenticationRepository& repository) noexcept
    : repository_(repository) {}

// ==========================================
// 密码验证实现
// ==========================================
AuthResult AuthService::verifyPassword(int user_id, const std::string& input_password) {

    const auto verification = repository_.verifyPassword(user_id, input_password);
    if (!verification) {
        return AuthResult::DB_ERROR;
    }
    if (verification.value() == smart_attendance::storage::PasswordVerification::NotFound) {
        return AuthResult::USER_NOT_FOUND;
    }
    if (verification.value() == smart_attendance::storage::PasswordVerification::NotConfigured) {
        return AuthResult::NO_FEATURE_DATA;
    }
    return verification.value() == smart_attendance::storage::PasswordVerification::Match
        ? AuthResult::SUCCESS
        : AuthResult::WRONG_PASSWORD;
}

// ==========================================
// 指纹验证实现
// ==========================================
AuthResult AuthService::verifyFingerprint(int user_id, const std::vector<uint8_t>& captured_fp_data) {
    const auto storedResult = repository_.fingerprintTemplate(user_id);
    if (!storedResult) {
        return AuthResult::DB_ERROR;
    }
    if (!storedResult.value()) {
        return AuthResult::USER_NOT_FOUND;
    }
    const auto& stored = *storedResult.value();
    if (stored.empty()) {
        return AuthResult::NO_FEATURE_DATA; // 用户未录入指纹
    }

    // 5. 执行指纹比对算法
    int score = matchFingerprintTemplate(stored, captured_fp_data);

    // 6. 判断得分是否通过 (假设阈值为 80 分)
    if (score >= 80) {
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

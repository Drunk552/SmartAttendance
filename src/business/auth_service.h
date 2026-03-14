#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>

// 验证结果枚举
enum class AuthResult {
    SUCCESS,           // 验证成功
    USER_NOT_FOUND,    // 用户不存在
    WRONG_PASSWORD,    // 密码错误
    WRONG_FINGERPRINT, // 指纹不匹配
    NO_FEATURE_DATA,   // 用户未录入指纹/密码
    DB_ERROR           // 数据库错误
};

/**
 * @brief 认证服务类
 * @details 只负责身份认证（密码/指纹验证）。
 *          认证成功后的考勤记录逻辑由 AttendanceRule::recordAttendance 负责。
 */
class AuthService {
public:
    /**
     * @brief 密码验证 (1:1)
     * @param user_id 用户工号
     * @param input_password 用户输入的密码字符串
     * @return AuthResult 验证结果
     */
    static AuthResult verifyPassword(int user_id, const std::string& input_password);

    /**
     * @brief 指纹验证 (1:1)
     * @param user_id 用户工号
     * @param captured_fp_data 采集到的指纹特征数据
     * @return AuthResult 验证结果
     */
    static AuthResult verifyFingerprint(int user_id, const std::vector<uint8_t>& captured_fp_data);

private:
    // 模拟指纹算法比对函数 (实际开发需替换为厂商SDK)
    static int matchFingerprintTemplate(const std::vector<uint8_t>& stored, const std::vector<uint8_t>& captured);
};

#endif // AUTH_SERVICE_H
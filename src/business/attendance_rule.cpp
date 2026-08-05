#include "attendance_rule.h"

#include "core/attendance/punch_rule.h"

#include <ctime>
#include <optional>

namespace {

std::optional<smart_attendance::core::PunchStatus> toCoreStatus(int status) noexcept {
    using CoreStatus = smart_attendance::core::PunchStatus;
    switch (status) {
        case 0:
            return CoreStatus::Normal;
        case 1:
            return CoreStatus::Late;
        case 2:
            return CoreStatus::Early;
        case 3:
            return CoreStatus::Absent;
        default:
            return std::nullopt;
    }
}

} // namespace

// 判断状态优先级 (状态值越小优先级越高：NORMAL=0，LATE=1，EARLY=2，ABSENT=3)
bool AttendanceRule::isStatusBetter(int new_status, int old_status) {
    const auto candidate = toCoreStatus(new_status);
    const auto current = toCoreStatus(old_status);
    if (!candidate || !current) {
        return new_status < old_status;
    }
    return smart_attendance::core::isPunchStatusBetter(*candidate, *current);
}

/**
 * @brief 辅助工具：将 "HH:MM" 字符串转换为当天的第 N 分钟 (0-1439)
 * 根据规则 Q5：解析前须完成容错清洗，处理以下异常格式：
 *   - 字符串前后空格 (如 " 09:00 ")
 *   - 全角中文冒号 (如 "9：00"，UTF-8 编码 0xEF 0xBC 0x9A)
 *   - 超出合法范围 (如 "24:00", "25:30")
 *   - 非法数字 (如 "_9:00", "09:-1")
 * @return 分钟数(0~1439)，解析失败返回 -1
 */
int AttendanceRule::timeStringToMinutes(const std::string& time_str) {
    const auto minutes = smart_attendance::core::parseFlexibleTimeToMinutes(time_str);
    return minutes.value_or(-1);
}

/**
 * @brief 判断打卡归属的班次（处理12:00-13:00 的折中原则）
 * @param punch_timestamp 打卡时间戳
 * @param shift_am 上午班次
 * @param shift_pm 下午班次
 * @return 1: 归属上午, 2: 归属下午
 */
int AttendanceRule::determineShiftOwner(time_t punch_timestamp, const ShiftConfig& shift_am, const ShiftConfig& shift_pm) {
    const std::tm* punchTime = std::localtime(&punch_timestamp);
    const int punchMinutes = punchTime->tm_hour * 60 + punchTime->tm_min;
    const int firstEndMinutes = timeStringToMinutes(shift_am.end_time);
    const int secondStartMinutes = timeStringToMinutes(shift_pm.start_time);

    const auto owner = smart_attendance::core::determineShiftOwner(
        punchMinutes, firstEndMinutes, secondStartMinutes);
    return owner == smart_attendance::core::ShiftOwner::FirstPeriod ? 1 : 2;
}

/**
 * @brief 计算具体状态（正常、迟到、早退、旷工）
 */
PunchResult AttendanceRule::calculatePunchStatus(time_t punch_timestamp, const ShiftConfig& target_shift, bool is_check_in) {
    const std::tm* punchTime = std::localtime(&punch_timestamp);
    const int punchMinutes = punchTime->tm_hour * 60 + punchTime->tm_min;
    const auto coreResult = smart_attendance::core::calculatePunchStatus(
        punchMinutes,
        timeStringToMinutes(target_shift.start_time),
        timeStringToMinutes(target_shift.end_time),
        target_shift.late_threshold_min,
        is_check_in);

    PunchStatus status = PunchStatus::NORMAL;
    switch (coreResult.status) {
        case smart_attendance::core::PunchStatus::Normal:
            status = PunchStatus::NORMAL;
            break;
        case smart_attendance::core::PunchStatus::Late:
            status = PunchStatus::LATE;
            break;
        case smart_attendance::core::PunchStatus::Early:
            status = PunchStatus::EARLY;
            break;
        case smart_attendance::core::PunchStatus::Absent:
            status = PunchStatus::ABSENT;
            break;
    }
    return PunchResult{status, coreResult.minutesDifference};
}

#include "attendance_rule.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath> // 用于数学计算

/**
 * @brief 辅助工具：将 "HH:MM" 字符串转换为当天的第 N 分钟 (0-1439)
 * 优化建议：避免重复的 sscanf/stringstream 解析，提高复用性 (参考任务书 4.2)
 */
int AttendanceRule::timeStringToMinutes(const std::string& time_str) {
    int hour = 0, min = 0;
    char sep;
    std::istringstream ss(time_str);
    if (ss >> hour >> sep >> min) {
        return hour * 60 + min;
    }
    return 0; // 解析失败默认返回0
}

/**
 * @brief 判断打卡归属的班次（处理12:00-13:00 的折中原则）
 * @return 1: 归属上午, 2: 归属下午
 */
int AttendanceRule::determineShiftOwner(time_t punch_timestamp, const ShiftConfig& shift_am, const ShiftConfig& shift_pm) {
    std::tm* punch_tm = std::localtime(&punch_timestamp);
    int punch_minutes = punch_tm->tm_hour * 60 + punch_tm->tm_min;

    // 1. 统一转换为分钟数进行比较
    int am_end_minutes = timeStringToMinutes(shift_am.end_time);
    int pm_start_minutes = timeStringToMinutes(shift_pm.start_time);

    // 2. 逻辑判定
    // 情况A: 明确在上午下班时间之前 -> 归属上午
    if (punch_minutes <= am_end_minutes) {
        return 1; 
    }
    // 情况B: 明确在下午上班时间之后 -> 归属下午
    else if (punch_minutes >= pm_start_minutes) {
        return 2;
    }
    // 情况C: 处于模糊地带（中间休息时间），应用“折中原则” (Story 1.1)
    else {
        // 计算折中点 (Midpoint Rule)
        // 公式: 中点 = 上午下班 + (下午上班 - 上午下班) / 2
        // 例如: 12:00 + (60分钟 / 2) = 12:30
        int mid_point = am_end_minutes + (pm_start_minutes - am_end_minutes) / 2;
        
        if (punch_minutes <= mid_point) {
            return 1; // 上半段归属上午（如：上午延迟签退）
        } else {
            return 2; // 下半段归属下午（如：下午提前签到）
        }
    }
}

/**
 * @brief 计算具体状态和分钟数 (Story 1.2)
 * @return PunchResult 包含状态(status)和差异时间(minutes_diff)
 */
PunchResult AttendanceRule::calculatePunchStatus(time_t punch_timestamp, const ShiftConfig& target_shift, bool is_check_in) {
    std::tm* punch_tm = std::localtime(&punch_timestamp);
    int punch_minutes = punch_tm->tm_hour * 60 + punch_tm->tm_min;

    int shift_start = timeStringToMinutes(target_shift.start_time);
    int shift_end = timeStringToMinutes(target_shift.end_time);
    
    // 初始化结果：默认为正常，差异为0
    PunchResult result = {PunchStatus::NORMAL, 0};

    if (is_check_in) {
        // ====================
        // 上班打卡逻辑 (Check In)
        // ====================
        if (punch_minutes <= shift_start) {
            // 正常：在上班时间之前或准点
            result.status = PunchStatus::NORMAL;
            result.minutes_diff = 0;
        } else {
            // 迟到或旷工
            int late_mins = punch_minutes - shift_start;
            result.minutes_diff = late_mins; // 记录具体的迟到分钟数

            if (late_mins <= target_shift.late_threshold_min) {
                result.status = PunchStatus::LATE;   // 允许范围内 -> 迟到
            } else {
                result.status = PunchStatus::ABSENT; // 超过阈值 -> 旷工 (严重迟到)
            }
        }
    } else {
        // ====================
        // 下班打卡逻辑 (Check Out)
        // ====================
        if (punch_minutes >= shift_end) {
            // 正常：在下班时间之后或准点
            result.status = PunchStatus::NORMAL;
            result.minutes_diff = 0;
        } else {
            // 早退
            result.status = PunchStatus::EARLY;
            // 记录具体的早退分钟数 (下班时间 - 打卡时间)
            result.minutes_diff = shift_end - punch_minutes; 
        }
    }

    return result;
}
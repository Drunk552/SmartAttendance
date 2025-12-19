#include "attendance_rule.h"
#include <sstream>
#include <iomanip>
#include <ctime>

/**
 * @brief 判断打卡归属的班次（处理12:00-13:00 的折中原则）
 * @param punch_timestamp 打卡时间戳
 * @param shift_am 上午班次
 * @param shift_pm 下午班次
 * @return 1: 归属上午, 2: 归属下午
 */
int AttendanceRule::determineShiftOwner(time_t punch_timestamp, const ShiftConfig& shift_am, const ShiftConfig& shift_pm) {
    std::tm* punch_tm = std::localtime(&punch_timestamp);
    int punch_minutes = punch_tm->tm_hour * 60 + punch_tm->tm_min;

    // 解析上午班次时间
    std::istringstream am_start_ss(shift_am.start_time);
    std::istringstream am_end_ss(shift_am.end_time);
    int am_start_hour, am_start_min, am_end_hour, am_end_min;
    char sep;
    am_start_ss >> am_start_hour >> sep >> am_start_min;
    am_end_ss >> am_end_hour >> sep >> am_end_min;
    int am_start_minutes = am_start_hour * 60 + am_start_min;
    int am_end_minutes = am_end_hour * 60 + am_end_min;

    // 解析下午班次时间
    std::istringstream pm_start_ss(shift_pm.start_time);
    std::istringstream pm_end_ss(shift_pm.end_time);
    int pm_start_hour, pm_start_min, pm_end_hour, pm_end_min;
    pm_start_ss >> pm_start_hour >> sep >> pm_start_min;
    pm_end_ss >> pm_end_hour >> sep >> pm_end_min;
    int pm_start_minutes = pm_start_hour * 60 + pm_start_min;
    int pm_end_minutes = pm_end_hour * 60 + pm_end_min;

    // 判断打卡时间归属
    if (punch_minutes >= am_start_minutes && punch_minutes <= am_end_minutes) {
        return 1; // 上午班次
    } else if (punch_minutes >= pm_start_minutes && punch_minutes <= pm_end_minutes) {
        return 2; // 下午班次
    } else if (punch_minutes > am_end_minutes && punch_minutes < pm_start_minutes) {
        // 折中原则，12:00-13:00视为上午班次
        return 1;
    }

    // 默认归属上午班次
    return 1;
}

/**
 * @brief 计算具体状态（正常、迟到、早退、旷工）
 * @param punch_timestamp 打卡时间戳
 * @param target_shift 目标班次配置
 * @param is_check_in 是否为上班打卡
 * @return 打卡状态枚举
 */
PunchStatus AttendanceRule::calculatePunchStatus(time_t punch_timestamp, const ShiftConfig& target_shift, bool is_check_in) {
    std::tm* punch_tm = std::localtime(&punch_timestamp);
    int punch_minutes = punch_tm->tm_hour * 60 + punch_tm->tm_min;

    // 解析班次时间
    std::istringstream start_ss(target_shift.start_time);
    std::istringstream end_ss(target_shift.end_time);
    int start_hour, start_min, end_hour, end_min;
    char sep;
    start_ss >> start_hour >> sep >> start_min;
    end_ss >> end_hour >> sep >> end_min;
    int start_minutes = start_hour * 60 + start_min;
    int end_minutes = end_hour * 60 + end_min;
    
    if (is_check_in) {

        // 计算上班打卡状态
        if (punch_minutes <= start_minutes) {
            return PunchStatus::NORMAL; // 正常
        } else if (punch_minutes <= start_minutes + target_shift.late_threshold_min) {
            return PunchStatus::LATE; // 迟到
        } else {
            return PunchStatus::ABSENT; // 旷工
        }
    } 
    
    else {
        // 计算下班打卡状态
        if (punch_minutes >= end_minutes) {
            return PunchStatus::NORMAL; // 正常
        } else {
            return PunchStatus::EARLY; // 早退
        }
    }
}
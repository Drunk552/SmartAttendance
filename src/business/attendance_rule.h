#ifndef ATTENDANCE_RULE_H
#define ATTENDANCE_RULE_H

#include <string>
#include <ctime>

//打卡状态枚举
enum class PunchStatus {
    NORMAL,     // 正常
    LATE,       // 迟到
    EARLY,      // 早退
    ABSENT      // 旷工
};

struct ShiftConfig{
    std::string start_time; // "09:00"
    std::string end_time;   // "18:00"
int late_threshold_min; // 允许迟到分钟数
};

class AttendanceRule {
public:

/**
 * /**
* @brief 判断打卡归属的班次（处理12:00-13:00 的折中原则）
* @param punch_timestamp 打卡时间戳
* @param shift_am 上午班次
* @param shift_pm 下午班次
* @return 1: 归属上午, 2: 归属下午
 */
static int determineShiftOwner(time_t punch_timestamp, const ShiftConfig& shift_am, const ShiftConfig& shift_pm) ;// 判断打卡归属班次

/**
 * * @brief 计算具体状态（正常、迟到、早退、旷工）
 */
static PunchStatus calculatePunchStatus(time_t punch_timestamp, const ShiftConfig& target_shift,bool is_check_in);// 计算打卡状态
};

#endif // ATTENDANCE_RULE_H

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
 * @param punch_timestamp 打卡时间戳
 * @param shift_am 上午班次
 * @param shift_pm 下午班次
 * @return 1: 归属上午, 2: 归属下午
 */
int AttendanceRule::determineShiftOwner(time_t punch_timestamp, const ShiftConfig& shift_am, const ShiftConfig& shift_pm) {
    std::tm* punch_tm = std::localtime(&punch_timestamp);
    int punch_minutes = punch_tm->tm_hour * 60 + punch_tm->tm_min;

    int am_end_minutes = timeStringToMinutes(shift_am.end_time);
    int pm_start_minutes = timeStringToMinutes(shift_pm.start_time);

    // [Fix] 处理跨天逻辑：如果下午班开始时间比上午下班还早（数值上），说明跨天了
    // 例如：AM结束 23:00 (1380), PM开始 00:00 (0)
    // 此时 PM开始应视为 1440
    if (pm_start_minutes < am_end_minutes) {
        pm_start_minutes += 1440;
    }

    // [Fix] 如果打卡时间非常早（比如凌晨 01:00），且班次涉及跨天（范围很大），
    // 应该把打卡时间也视为“次日”（+1440），以便与上面调整过的时间进行比较。
    // 启发式规则：如果 punch 很小，而 am_end 很大，说明 punch 是次日凌晨
    if (am_end_minutes > 1000 && punch_minutes < 480) { // 480 = 08:00
        punch_minutes += 1440;
    }

    // 2. 逻辑判定
    // 情况A: 明确在上午下班时间之前 -> 归属上午
    if (punch_minutes <= am_end_minutes) {
        return 1; 
    }
    // 情况B: 明确在下午上班时间之后 -> 归属下午
    else if (punch_minutes >= pm_start_minutes) {
        return 2;
    }
    // 情况C: 处于模糊地带，应用“折中原则”
    else {
        int mid_point = am_end_minutes + (pm_start_minutes - am_end_minutes) / 2;
        if (punch_minutes <= mid_point) {
            return 1; 
        } else {
            return 2; 
        }
    }
}

/**
 * @brief 计算具体状态（正常、迟到、早退、旷工）
 */
PunchResult AttendanceRule::calculatePunchStatus(time_t punch_timestamp, const ShiftConfig& target_shift, bool is_check_in) {
    std::tm* punch_tm = std::localtime(&punch_timestamp);
    int punch_minutes = punch_tm->tm_hour * 60 + punch_tm->tm_min;

    int shift_start = timeStringToMinutes(target_shift.start_time);
    int shift_end = timeStringToMinutes(target_shift.end_time);
    
    // [Fix 1] 标准化班次时间
    // 如果 结束时间 < 开始时间，说明跨天了 (例如 22:00 - 06:00)
    // 此时将结束时间 + 24小时 (1440分钟)
    if (shift_end < shift_start) {
        shift_end += 1440;
    }

    // [Fix 2] 标准化打卡时间
    // 如果班次跨天 (shift_end > 1440)，且打卡时间是凌晨 (比如 01:00 = 60)，
    // 那么这个 60 应该被视为 25:00 (1500) 才能正确比较。
    // 
    // 判定规则：如果 shift_start 在很晚 (比如 > 18:00/1080分)，
    // 且 punch 在很早 (比如 < 12:00/720分)，则认为是次日打卡。
    if (shift_start > 1080 && punch_minutes < 720) {
        punch_minutes += 1440;
    }

    // 初始化结果
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
            result.minutes_diff = late_mins; 

            if (late_mins <= target_shift.late_threshold_min) {
                result.status = PunchStatus::LATE;   // 允许范围内 -> 迟到
            } else {
                result.status = PunchStatus::ABSENT; // 超过阈值 -> 旷工
            }
        }
    } else {
        // ====================
        // 下班打卡逻辑 (Check Out)
        // ====================
        // 注意：这里的 shift_end 可能是大于 1440 的数值
        if (punch_minutes >= shift_end) {
            // 正常：在下班时间之后或准点
            result.status = PunchStatus::NORMAL;
            result.minutes_diff = 0;
        } else {
            // 早退
            result.status = PunchStatus::EARLY;
            // 记录具体的早退分钟数
            result.minutes_diff = shift_end - punch_minutes; 
        }
    }

    return result;
}
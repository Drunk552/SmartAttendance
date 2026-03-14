#include "attendance_rule.h"
#include "../data/db_storage.h" // 考勤记录入库接口
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>    // 用于数学计算
#include <cctype>   // 用于 isdigit
#include <algorithm>// 用于 trim

// 判断状态优先级 (状态值越小优先级越高：NORMAL=0，LATE=1，EARLY=2，ABSENT=3)
bool AttendanceRule::isStatusBetter(int new_status, int old_status) {
    return new_status < old_status; 
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
    if (time_str.empty()) return -1;

    // [Q5 容错1] 去除首尾空格（含全角空格 0xE3 0x80 0x80）
    std::string s = time_str;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end());

    if (s.empty()) return -1;

    // [Q5 容错2] 将全角中文冒号（UTF-8: 0xEF 0xBC 0x9A）替换为半角冒号
    // 同时处理全角句号（0xE3 0x80 0x82）、中文点（0xC2 0xB7）等常见混淆字符
    std::string cleaned;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c0 = (unsigned char)s[i];
        // 全角冒号 ：(EF BC 9A)
        if (i + 2 < s.size() &&
            c0 == 0xEF && (unsigned char)s[i+1] == 0xBC && (unsigned char)s[i+2] == 0x9A) {
            cleaned += ':';
            i += 3;
        }
        // 全角句号 。(E3 80 82)
        else if (i + 2 < s.size() &&
            c0 == 0xE3 && (unsigned char)s[i+1] == 0x80 && (unsigned char)s[i+2] == 0x82) {
            cleaned += ':';
            i += 3;
        }
        // 中文间隔号 · (C2 B7)
        else if (i + 1 < s.size() &&
            c0 == 0xC2 && (unsigned char)s[i+1] == 0xB7) {
            cleaned += ':';
            i += 2;
        }
        else {
            cleaned += s[i];
            ++i;
        }
    }
    s = cleaned;

    // [Q5 容错3-ext] 去除首尾残留的非数字、非冒号字符
    // 处理 "_9:00"、"#09:00"、"09:00." 等前后缀污染字符
    {
        size_t start = s.find_first_of("0123456789");
        size_t end   = s.find_last_of("0123456789");
        if (start == std::string::npos) return -1;
        s = s.substr(start, end - start + 1);
    }

    // [Q5 容错4-ext] 将点号(.)、横杠(-)、空格( ) 等常见分隔符替换为冒号
    // 处理 "09.00"、"09-00"、"9 00" 等
    for (char& c : s) {
        if (c == '.' || c == '-' || c == ' ') {
            c = ':';
        }
    }

    // [Q5 容错5-ext] 处理无分隔符的纯数字格式（3~4位）
    // "0900" → "09:00"，"900" → "09:00"，"9" → "09:00"
    {
        size_t colon_pos = s.find(':');
        if (colon_pos == std::string::npos) {
            // 纯数字，尝试按位数解析
            bool all_digit = true;
            for (unsigned char c : s) {
                if (!std::isdigit(c)) { all_digit = false; break; }
            }
            if (all_digit) {
                if (s.size() == 4) {
                    // "0900" → "09:00"
                    s = s.substr(0, 2) + ":" + s.substr(2, 2);
                } else if (s.size() == 3) {
                    // "900" → "09:00"（首位为小时，后两位为分钟）
                    s = "0" + s.substr(0, 1) + ":" + s.substr(1, 2);
                } else if (s.size() <= 2) {
                    // "9" 或 "09" → "09:00"（只有小时，分钟默认00）
                    s = s + ":00";
                } else {
                    return -1; // 5位以上纯数字，无法识别
                }
            } else {
                return -1; // 含非数字字符且无分隔符，无法识别
            }
        }
    }

    // [Q5 容错6] 查找冒号分隔符，提取小时和分钟
    size_t colon_pos = s.find(':');
    if (colon_pos == std::string::npos) return -1;

    std::string hour_str = s.substr(0, colon_pos);
    std::string min_str  = s.substr(colon_pos + 1);

    // 去除小时/分钟字段内残留空格（防止 " 9 : 0 0 " 这类极端输入）
    hour_str.erase(std::remove_if(hour_str.begin(), hour_str.end(), ::isspace), hour_str.end());
    min_str.erase(std::remove_if(min_str.begin(), min_str.end(), ::isspace), min_str.end());

    // 校验每个字符必须是数字（拦截 "_9:00", "09:-1" 等无法转换的非法符号）
    auto isAllDigit = [](const std::string& str) -> bool {
        if (str.empty()) return false;
        for (unsigned char c : str) {
            if (!std::isdigit(c)) return false;
        }
        return true;
    };
    if (!isAllDigit(hour_str) || !isAllDigit(min_str)) return -1;

    int hour = std::stoi(hour_str);
    int min  = std::stoi(min_str);

    // [Q5 容错7] 校验时间数值范围（拦截 "24:00", "25:30", "09:61" 等真正越界值）
    // 注意：24:00 这类确实无法转换，直接拒绝
    if (hour > 23 || min > 59) return -1;

    return hour * 60 + min;
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

// ======================================================
// 考勤记录入口（验证成功后由 UI 层调用）
// 时序对应流程图：获取排班 -> 节点K -> 考勤计算 -> 入库
// ======================================================

RecordResult AttendanceRule::recordAttendance(int user_id, const cv::Mat& image) {
    // ======================================================
    // 第一阶段：初始化 - 获取当天排班
    // db_get_user_shift_smart 内部包含完整优先级链：
    //   Excel个人排班 > 部门周排班 > 默认班次
    //   + 【节点K】周六/周日是否上班规则检查
    // ======================================================
    long long now_ts = static_cast<long long>(std::time(nullptr));
    auto shift_opt   = db_get_user_shift_smart(user_id, now_ts);

    // 无排班（含周末不上班），不记录，结束
    if (!shift_opt.has_value() || shift_opt.value().id == 0) {
        return RecordResult::NO_SHIFT;
    }

    const ShiftInfo& shift = shift_opt.value();

    // ======================================================
    // 防重复打卡检查
    // ======================================================
    RuleConfig rules    = db_get_global_rules();
    int dup_limit_sec   = rules.duplicate_punch_limit * 60;
    if (dup_limit_sec > 0) {
        auto recent = db_get_records_by_user(user_id, now_ts - dup_limit_sec, now_ts);
        if (!recent.empty()) {
            return RecordResult::DUPLICATE_PUNCH;
        }
    }

    // ======================================================
    // 第二阶段：打卡归属判断（折中原则）
    // ======================================================
    ShiftConfig shift_am;
    shift_am.start_time         = shift.s1_start;
    shift_am.end_time           = shift.s1_end;
    shift_am.late_threshold_min = rules.late_threshold;

    ShiftConfig shift_pm;
    shift_pm.start_time         = shift.s2_start;
    shift_pm.end_time           = shift.s2_end;
    shift_pm.late_threshold_min = rules.late_threshold;

    // 折中原则：判断打卡归属上班还是下班
    int  shift_owner = determineShiftOwner(static_cast<time_t>(now_ts), shift_am, shift_pm);
    bool is_check_in = (shift_owner == 1);

    // ======================================================
    // 第三阶段：状态判定（正常/迟到/早退/旷工）
    // ======================================================
    PunchResult pr = calculatePunchStatus(
        static_cast<time_t>(now_ts),
        is_check_in ? shift_am : shift_pm,
        is_check_in);

    // PunchStatus -> DB 存储的 int 状态码
    int db_status = 0;
    switch (pr.status) {
        case PunchStatus::NORMAL: db_status = 0; break;
        case PunchStatus::LATE:   db_status = 1; break;
        case PunchStatus::EARLY:  db_status = 2; break;
        case PunchStatus::ABSENT: db_status = 3; break;
    }

    // ======================================================
    // 写入数据库
    // ======================================================
    bool ok = db_log_attendance(user_id, shift.id, image, db_status);
    if (!ok) {
        return RecordResult::DB_ERROR;
    }

    // 返回考勤语义结果，供 UI 层显示对应提示
    switch (pr.status) {
        case PunchStatus::NORMAL: return RecordResult::RECORDED_NORMAL;
        case PunchStatus::LATE:   return RecordResult::RECORDED_LATE;
        case PunchStatus::EARLY:  return RecordResult::RECORDED_EARLY;
        case PunchStatus::ABSENT: return RecordResult::RECORDED_ABSENT;
        default:                  return RecordResult::RECORDED_NORMAL;
    }
}
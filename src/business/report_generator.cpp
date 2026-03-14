/**
 * @file report_generator.cpp
 * @brief 报表导出实现
 */

#include "report_generator.h"
#include <xlsxwriter.h>
#include <iostream>
#include <map>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <sys/stat.h> // 用于创建目录
#include <db_storage.h>// 调用数据层接口
#include "attendance_rule.h"

// 辅助函数：判断新打卡记录的状态是否比旧记录更“好” (正常优先原则)
// 返回 true 表示新记录优先级更高，应该替换旧记录
bool isStatusBetter(int new_status, int old_status) {
    // 假设状态枚举：STATUS_NORMAL = 0, STATUS_LATE = 1, STATUS_EARLY = 2, STATUS_ABSENT = 3
    // 状态值越小，代表考勤越正常 (0是最高优)
    // 如果都是异常（比如都是迟到），优先级一样，由时间早晚来决定
    return new_status < old_status; 
}

/**
 * @brief 将时间字符串 "HH:MM" 转换为分钟数
 * 根据规则 Q5：实施容错清洗，处理以下异常情况：
 *   - 字符串前后空格、全角冒号、非法字符、超界时间均返回 -1
 * @param time_str 时间字符串
 * @return 分钟数(0~1439)，解析失败返回 -1
 */
int timeStrToMinutes(const std::string& time_str) {
    // 直接将容错逗托给 AttendanceRule::timeStringToMinutes
    // 保证全局解析逻辑一致，避免两处代码分岐 (Q5)
    return AttendanceRule::timeStringToMinutes(time_str);
}

/**
 * @brief 构造函数
 */
ReportGenerator::ReportGenerator() {}
ReportGenerator::~ReportGenerator() {}

/**
 * @brief 将日期字符串转换为时间戳
 * @param date_str 日期字符串 "YYYY-MM-DD"
 * @param is_end_of_day 是否转换为当天结束时间 (23:59:59)
 * @return 对应的时间戳 (秒级)
 */
long long ReportGenerator::parseDateToTimestamp(const std::string& date_str, bool is_end_of_day) {

    std::tm tm = {};
    std::stringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    
     if (ss.fail()) {
        ss.clear();
        ss.str(date_str);
        ss >> std::get_time(&tm, "%Y/%m/%d");
    }
    
    if (ss.fail()) {
        std::cerr << "[Error] 无法解析日期: " << date_str << std::endl;
        return 0;
    }
    
    if (is_end_of_day) {
        tm.tm_hour = 23; 
        tm.tm_min = 59; 
        tm.tm_sec = 59;
    } else {
        tm.tm_hour = 0; 
        tm.tm_min = 0; 
        tm.tm_sec = 0;
    }
    
    return std::mktime(&tm);
}

/**
 * @brief 将时间戳格式化为 "HH:MM"
 * @param timestamp 时间戳 (秒级)
 * @return 格式化字符串 "HH:MM"
 */
std::string ReportGenerator::formatTime(long long timestamp) {
    if (timestamp == 0) return "--:--";
    std::time_t t = (std::time_t)timestamp;
    std::tm *tm = std::localtime(&t);
    if (!tm) return "--:--";
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", tm);
    return std::string(buf);
}

/**
 * @brief 将时间戳格式化为 "YYYY-MM-DD"
 * @param timestamp 时间戳 (秒级)
 * @return 格式化字符串 "YYYY-MM-DD"
 */
std::string ReportGenerator::formatDate(long long timestamp) {
    if (timestamp == 0) return "0000-00-00";
    std::time_t t = (std::time_t)timestamp;
    std::tm *tm = std::localtime(&t);
    if (!tm) return "0000-00-00";
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return std::string(buf);
}

/**
 * @brief 将年份和月份格式化为 "YYYY-MM"
 * @param year 年份
 * @param month 月份
 * @return 格式化字符串 "YYYY-MM"
 */
std::string ReportGenerator::formatMonth(int year, int month) {
    std::stringstream ss;
    ss << year << "-" << std::setw(2) << std::setfill('0') << month;
    return ss.str();
}

/**
 * @brief 获取指定月份的天数
 * @param year 年份
 * @param month 月份
 * @return 天数
 */
int ReportGenerator::getDaysInMonth(int year, int month) {
    if (month < 1 || month > 12) return 31;
    
    const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int days = days_in_month[month - 1];
    
    // 闰年二月
    if (month == 2) {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (is_leap) days = 29;
    }
    
    return days;
}

/**
 * @brief 从时间戳中提取日
 * @param timestamp 时间戳 (秒级)
 * @return 日 (1-31)
 */
int ReportGenerator::extractDayFromTimestamp(long long timestamp) {
    if (timestamp == 0) return 0;
    std::time_t t = (std::time_t)timestamp;
    std::tm* tm = std::localtime(&t);
    return tm ? tm->tm_mday : 0;
}

/**
 * @brief 从时间戳中提取月
 * @param timestamp 时间戳 (秒级)
 * @return 月 (1-12)
 */
int ReportGenerator::extractMonthFromTimestamp(long long timestamp) {
    if (timestamp == 0) return 0;
    std::time_t t = (std::time_t)timestamp;
    std::tm* tm = std::localtime(&t);
    return tm ? tm->tm_mon + 1 : 0;
}

/**
 * @brief 从时间戳中提取年
 * @param timestamp 时间戳 (秒级)
 * @return 年 (四位数)
 */
int ReportGenerator::extractYearFromTimestamp(long long timestamp) {
    if (timestamp == 0) return 0;
    std::time_t t = (std::time_t)timestamp;
    std::tm* tm = std::localtime(&t);
    return tm ? tm->tm_year + 1900 : 0;
}

/**
 * @brief 动态计算迟到分钟数
 * @param user_id 用户ID
 * @param timestamp 打卡时间戳
 * @return 迟到分钟数
 */
int ReportGenerator::calculateLateMinutes(long long timestamp, const ShiftInfo& shift) {
    // 1. 获取打卡时间的分钟数
    std::time_t t = (std::time_t)timestamp;
    std::tm* tm = std::localtime(&t);
    int punch_mins = tm->tm_hour * 60 + tm->tm_min;

    // 2. 解析班次时间点
    int s1_start = timeStrToMinutes(shift.s1_start); 
    int s1_end   = timeStrToMinutes(shift.s1_end);
    int s2_start = timeStrToMinutes(shift.s2_start); 

    // 3. [Epic 4.4 优化] 动态计算分界点
    // 默认为 12:00 (720)，防止没有下午班次时出错
    int split_point = 720; 
    
    // 如果上午班结束和下午班开始都存在，取两者的中间点作为分界线
    // 例如：上午结束 12:00，下午开始 14:00 -> 分界点 13:00
    if (s1_end > 0 && s2_start > 0 && s2_start > s1_end) {
        split_point = s1_end + (s2_start - s1_end) / 2;
    }

    int late_mins = 0;

    // 4. 判定逻辑
    // 情况 A: 打卡时间在分界点之前 -> 归属上午班，跟 s1_start 比
    if (s1_start != -1 && punch_mins <= split_point) { 
        if (punch_mins > s1_start) {
            late_mins = punch_mins - s1_start;
        }
    }
    // 情况 B: 打卡时间在分界点之后 -> 归属下午班，跟 s2_start 比
    else if (s2_start != -1 && punch_mins > split_point) {
        if (punch_mins > s2_start) {
            late_mins = punch_mins - s2_start;
        }
    }
    
    return late_mins > 0 ? late_mins : 0;
}

/**
 * @brief 动态计算早退分钟数
 * @param user_id 用户ID
 * @param timestamp 打卡时间戳
 * @return 早退分钟数
 */
int ReportGenerator::calculateEarlyMinutes(long long timestamp, const ShiftInfo& shift) {
    std::time_t t = (std::time_t)timestamp;
    std::tm* tm = std::localtime(&t);
    int punch_mins = tm->tm_hour * 60 + tm->tm_min;

    int s1_end   = timeStrToMinutes(shift.s1_end); 
    int s2_start = timeStrToMinutes(shift.s2_start);
    int s2_end   = timeStrToMinutes(shift.s2_end); 
    
    // [Epic 4.4 优化] 动态计算分界点 (逻辑同上)
    int split_point = 720; // 默认 12:00
    
    if (s1_end > 0 && s2_start > 0 && s2_start > s1_end) {
        split_point = s1_end + (s2_start - s1_end) / 2;
    }

    int early_mins = 0;

    // 判定逻辑：
    // 情况 A: 打卡时间在分界点之前 -> 视为上午下班，跟 s1_end 比
    if (s1_end != -1 && punch_mins <= split_point) { 
        if (punch_mins < s1_end) {
            early_mins = s1_end - punch_mins;
        }
    }
    // 情况 B: 打卡时间在分界点之后 -> 视为下午下班，跟 s2_end 比
    else if (s2_end != -1 && punch_mins > split_point) {
        if (punch_mins < s2_end) {
            early_mins = s2_end - punch_mins;
        }
    }

    return early_mins > 0 ? early_mins : 0;
}

// ==================== 数据库访问函数 ====================

/**
 * @brief 从数据库获取指定时间范围内的考勤记录
 * @param start_ts 起始时间戳 (秒级)
 * @param end_ts 结束时间戳 (秒级)
 * @return 考勤记录列表
 */
std::vector<AttendanceRecord> ReportGenerator::db_get_records(long long start_ts, long long end_ts) {
    std::vector<AttendanceRecord> records;
    auto db_records = ::db_get_records(start_ts, end_ts);

    // 班次缓存 map，避免同一个人同一天重复查询数据库
    // Key: "UserID_YYYY-MM-DD", Value: ShiftInfo
    std::map<std::string, ShiftInfo> shift_cache;
    
    for (const auto& record : db_records) {
        AttendanceRecord rec; // 复制数据
        rec.id = record.id; 
        rec.user_id = record.user_id; 
        rec.timestamp = record.timestamp; 
        rec.status = record.status; 
        rec.user_name = record.user_name; 
        rec.dept_name = record.dept_name; 
        rec.image_path = record.image_path;

        rec.minutes_late = 0; // 默认值为0
        rec.minutes_early = 0; // 默认值为0

        // 动态获取班次信息
        std::string date_str = formatDate(rec.timestamp);
        std::string cache_key = std::to_string(rec.user_id) + "_" + date_str;

        ShiftInfo current_shift;
        // 检查缓存或查询数据库
        if (shift_cache.find(cache_key) != shift_cache.end()) {
            current_shift = shift_cache[cache_key];
        } 
        else {
            // 智能获取当天的排班
            auto shift_opt = ::db_get_user_shift_smart(rec.user_id, rec.timestamp);
            
            if (shift_opt.has_value()) {
                current_shift = shift_opt.value(); // 如果有排班，把数据拆盒取出来
            } else {
                current_shift = ShiftInfo(); // 如果是空盒子(休息日)，构造一个默认空对象
                current_shift.id = 0;        // 确保 ID 为 0，兼容后续的迟到早退计算逻辑
            }

            shift_cache[cache_key] = current_shift;
        } 
        
        // 计算迟到/早退逻辑 (自动处理 id=0 的情况)
        int late_min = calculateLateMinutes(rec.timestamp, current_shift);
        int early_min = calculateEarlyMinutes(rec.timestamp, current_shift);

        // 根据计算结果强制更新状态和分钟数
        if (late_min > 0) {
            rec.status = STATUS_LATE;       // 强制标记为迟到
            rec.minutes_late = late_min;    // 记录迟到时长
        } 
        else if (early_min > 0) {
            rec.status = STATUS_EARLY;      // 强制标记为早退
            rec.minutes_early = early_min;  // 记录早退时长
        }
        else {
            // 如果既不迟到也不早退，但原状态显示异常，则修正为正常
            if (rec.status == STATUS_LATE || rec.status == STATUS_EARLY) {
                rec.status = STATUS_NORMAL;
            }
        }
        records.push_back(rec);
    }
    
    std::cout << "[Report] 从数据库获取 " << records.size() 
              << " 条考勤记录 (" << formatDate(start_ts) 
              << " 到 " << formatDate(end_ts) << ")" << std::endl;
              
    return records;
}

/**
 * @brief 从数据库获取所有用户信息
 * @return 用户信息列表
 */
std::vector<UserData> ReportGenerator::db_get_all_users_info() {
    std::vector<UserData> users;
    auto db_users = ::db_get_all_users();

     for (const auto& db_user : db_users) {
        UserData user;
        user.id = db_user.id;
        user.name = db_user.name;
        user.password = db_user.password;
        user.card_id = db_user.card_id;
        user.role = db_user.role;
        user.dept_id = db_user.dept_id;
        user.dept_name = db_user.dept_name;
        user.face_feature = db_user.face_feature;
        user.position = ""; // 数据层没有职位信息，可设为空或根据角色判断

        // 根据privilege设置职位
        if (db_user.role == 1) {
            user.position = "管理员";
        } else {
            user.position = "员工";
        }
        
        users.push_back(user);
    }
    
    std::cout << "[Report] 从数据库获取 " << users.size() << " 个用户信息" << std::endl;
    
    return users;
}

/**
 * @brief 从数据库获取指定部门的用户列表
 * @param dept_name 部门名称
 * @return 用户信息列表
 */
std::vector<UserData> ReportGenerator::db_get_users_by_dept(const std::string& dept_name) {
    std::vector<UserData> users = db_get_all_users_info();
    std::vector<UserData> result;
    
    // 筛选指定部门的用户
    for (const auto& user : users) {
        if (user.dept_name == dept_name) {
            result.push_back(user);
        }
    }
    
    std::cout << "[Report] 从部门 '" << dept_name << "' 获取 " 
              << result.size() << " 个用户" << std::endl;
    
    return result;
}

// ==================== 样式创建函数 ====================
lxw_format* ReportGenerator::createHeaderFormat(lxw_workbook* workbook) {
    lxw_format* format = workbook_add_format(workbook);
    format_set_bold(format);
    format_set_bg_color(format, 0x366092); // 深蓝色背景
    format_set_font_color(format, LXW_COLOR_WHITE);
    format_set_border(format, LXW_BORDER_THIN);
    format_set_align(format, LXW_ALIGN_CENTER);
    return format;
}

lxw_format* ReportGenerator::createNormalFormat(lxw_workbook* workbook) {
    lxw_format* format = workbook_add_format(workbook);
    format_set_border(format, LXW_BORDER_THIN);
    format_set_align(format, LXW_ALIGN_CENTER);
    return format;
}

lxw_format* ReportGenerator::createRedFormat(lxw_workbook* workbook) {
    lxw_format* format = workbook_add_format(workbook);
    format_set_font_color(format, LXW_COLOR_RED); // Epic5.3: 红色字体
    format_set_border(format, LXW_BORDER_THIN);
    format_set_align(format, LXW_ALIGN_CENTER);
    return format;
}

lxw_format* ReportGenerator::createGreenFormat(lxw_workbook* workbook) {
    lxw_format* format = workbook_add_format(workbook);
    format_set_font_color(format, 0x00A933); // 绿色字体
    format_set_border(format, LXW_BORDER_THIN);
    format_set_align(format, LXW_ALIGN_CENTER);
    return format;
}

lxw_format* ReportGenerator::createYellowFormat(lxw_workbook* workbook) {
    lxw_format* format = workbook_add_format(workbook);
    format_set_font_color(format, 0xFF9900); // 黄色/橙色字体
    format_set_border(format, LXW_BORDER_THIN);
    format_set_align(format, LXW_ALIGN_CENTER);
    return format;
}

// ==================== 数据辅助函数 ====================

/**
 * @brief 根据考勤状态获取显示符号
 * @param check_in 打卡时间字符串
 * @param check_out 打卡时间字符串
 * @param status 考勤状态
 * @return 显示符号字符串
 */
std::string ReportGenerator::getAttendanceSymbol(const std::string& check_in,  const std::string& check_out,  int status) {

    if (status == STATUS_ABSENT) return "A";
    // [规则 Q3-3.1.2] 未排班：显示 "-" 表示当天不参与考勤，与旷工 "A" 严格区分
    if (status == STATUS_NO_SHIFT) return "-";
    if (status == STATUS_LATE) {
        if (check_in != "--:--") return "L(" + check_in + ")";
        return "L";
    }
    if (status == STATUS_EARLY) {
        if (check_out != "--:--") return "E(" + check_out + ")";
        return "E";
    }
    if (status == STATUS_NORMAL) {
        if (check_in != "--:--") return check_in;
        return "✓";
    }
    return "?";
}

lxw_color_t ReportGenerator::getStatusColor(int status) {
    switch (status) {
        case STATUS_LATE:
        case STATUS_ABSENT:
            return LXW_COLOR_RED; // 红色字体
        case STATUS_EARLY:
            return 0xFF9900; // 橙色
        case STATUS_NORMAL:
            return 0x00A933; // 绿色
        case STATUS_NO_SHIFT:
            // [规则 Q3-3.1.2] 未排班：用浅灰色显示，在报表中明显区别于旷工的红色
            return 0x999999; // 灰色
        default:
            return LXW_COLOR_BLACK;
    }
}

// ==================== 核心数据处理函数 ====================

/**
 * @brief 处理考勤数据，生成日报和月报
 * @param records 考勤记录列表
 * @param users 用户信息列表
 * @param start_ts 起始时间戳 (秒级)
 * @param end_ts 结束时间戳 (秒级)
 * @param detail_data 输出：每日明细数据
 * @param summaries 输出：月度汇总数据
 */
void ReportGenerator::processAttendanceData(
    const std::vector<AttendanceRecord>& records,
    const std::vector<UserData>& users,
    long long start_ts, long long end_ts,
    std::map<int, std::map<int, DailyCellData>>& detail_data,
    std::map<int, MonthlySummary>& summaries) {
    
    // 初始化数据结构
    for (const auto& user : users) {
        detail_data[user.id] = std::map<int, DailyCellData>();
        
        MonthlySummary summary;
        summary.user_name = user.name;
        summary.user_code = std::to_string(user.id);
        summary.dept = user.dept_name;
        summaries[user.id] = summary;
    }

    // [规则 Q3] 从数据库获取全局考勤规则，获取允许迟到分钟阈値
    // 避免硬编码为 0、和数据库配置脱节
    RuleConfig global_rules = db_get_global_rules();
    int late_threshold = global_rules.late_threshold; // 全局迟到容忍分钟数
    
    // 处理考勤记录
    for (const auto& rec : records) {
        int user_id = rec.user_id;
        int day = extractDayFromTimestamp(rec.timestamp); // 获取几号
        std::string punch_time = formatTime(rec.timestamp); // 格式化为 HH:MM
        
        auto& cell = detail_data[user_id][day];

        // ==========================================
        // 1 & 2. 使用智能班次查询（流程图完整优先级链）
        // 替换旧逻辑（手动查 default_shift_id -> db_get_shift_info）
        // 改为调用 db_get_user_shift_smart，该接口内部完整实现了：
        //   - Excel个人特殊排班（最高优先）
        //   - 部门周排班
        //   - 默认班次 fallback
        //   - 【节点K】周六/周日是否上班规则检查
        // ==========================================
        auto day_shift_opt = db_get_user_shift_smart(user_id, rec.timestamp);

        // 若节点K判定当天无排班（如周末不上班），跳过此条打卡记录不计入考勤
        if (!day_shift_opt.has_value() || day_shift_opt.value().id == 0) {
            continue; // 无排班，忽略该打卡记录
        }

        ShiftConfig shift_am;
        ShiftConfig shift_pm;
        {
            const ShiftInfo& db_shift = day_shift_opt.value();
            // 组装上午班次规则
            shift_am.start_time        = db_shift.s1_start;
            shift_am.end_time          = db_shift.s1_end;
            shift_am.late_threshold_min = late_threshold; // [规则 Q3] 全局迟到阈值
            // 组装下午班次规则
            shift_pm.start_time        = db_shift.s2_start;
            shift_pm.end_time          = db_shift.s2_end;
            shift_pm.late_threshold_min = late_threshold; // [规则 Q3] 全局迟到阈值
        }

        // ==========================================
        // 3. 调用规则引擎判定折中阶段
        // ==========================================
        int shift_owner = AttendanceRule::determineShiftOwner(rec.timestamp, shift_am, shift_pm);
        bool is_morning_punch = (shift_owner == 1); // 1表示属于上班阶段(Check-in)

        // ==========================================
        // 4. 核心逻辑：正常优先、就近原则过滤
        // ==========================================
        if (is_morning_punch) {
            // --- 上班打卡 (Check-in) 过滤 ---
            if (cell.check_in.empty() || cell.check_in == "--:--") {
                cell.check_in = punch_time;
                cell.check_in_status = rec.status;
                cell.check_in_timestamp = rec.timestamp;
            } else {
                if (AttendanceRule::isStatusBetter(rec.status, cell.check_in_status)) {
                    cell.check_in = punch_time;
                    cell.check_in_status = rec.status;
                    cell.check_in_timestamp = rec.timestamp;
                } else if (rec.status == cell.check_in_status) {
                    if (rec.timestamp < cell.check_in_timestamp) { // 状态相同时取最早
                        cell.check_in = punch_time;
                        cell.check_in_status = rec.status;
                        cell.check_in_timestamp = rec.timestamp;
                    }
                }
            }
        } else {
            // --- 下班打卡 (Check-out) 过滤 ---
            if (cell.check_out.empty() || cell.check_out == "--:--") {
                cell.check_out = punch_time;
                cell.check_out_status = rec.status;
                cell.check_out_timestamp = rec.timestamp;
            } else {
                if (AttendanceRule::isStatusBetter(rec.status, cell.check_out_status)) {
                    cell.check_out = punch_time;
                    cell.check_out_status = rec.status;
                    cell.check_out_timestamp = rec.timestamp;
                } else if (rec.status == cell.check_out_status) {
                    if (rec.timestamp > cell.check_out_timestamp) { // 状态相同时取最晚
                        cell.check_out = punch_time;
                        cell.check_out_status = rec.status;
                        cell.check_out_timestamp = rec.timestamp;
                    }
                }
            }
        }
    }
    
    //补全缺失日期并统计
    int year = extractYearFromTimestamp(start_ts);
    int month = extractMonthFromTimestamp(start_ts);
    int days_in_month = getDaysInMonth(year, month);
    
    for (auto& [user_id, user_data] : detail_data) {
        MonthlySummary& summary = summaries[user_id];
        
        for (int day = 1; day <= days_in_month; day++) {
            if (user_data.find(day) == user_data.end()) {
                // [规则 Q3 第一阶段] 该天无打卡记录，需先判断是否排班：
                //   - 有排班且无打卡 -> 旷工 (STATUS_ABSENT)
                //   - 无排班（db_get_user_shift_smart 返回 id==0） -> 未排班 (STATUS_NO_SHIFT)
                //   两种情况是完全不同的业务语义，不能统一处理为旷工
                long long day_ts = parseDateToTimestamp(formatDateString(year, month, day), false);
                auto day_shift = db_get_user_shift_smart(user_id, day_ts);
                bool has_shift = day_shift.has_value() && day_shift.value().id != 0;

                DailyCellData empty_data;
                empty_data.user_name = summary.user_name;
                empty_data.user_code = summary.user_code;
                empty_data.check_in  = "--:--";
                empty_data.check_out = "--:--";

                if (has_shift) {
                    // [规则 Q3-3.1.3] 有排班但全天无打卡 -> 旷工
                    empty_data.status = STATUS_ABSENT;
                    user_data[day] = empty_data;
                    summary.absent_days++;
                } else {
                    // [规则 Q3-3.1.2] 无排班 -> 未排班，不计入旷工
                    empty_data.status = STATUS_NO_SHIFT;
                    user_data[day] = empty_data;
                    summary.no_shift_days++;
                }
            } else {
                DailyCellData& cell_data = user_data[day];
                switch (cell_data.status) {
                    case STATUS_NORMAL:
                        summary.normal_days++;
                        break;
                    case STATUS_LATE:
                        summary.late_count++;
                        summary.total_late_minutes += cell_data.late_minutes;
                        break;
                    case STATUS_EARLY:
                        summary.early_count++;
                        summary.total_early_minutes += cell_data.early_minutes;
                        break;
                    case STATUS_ABSENT:
                        summary.absent_days++;
                        break;
                    case STATUS_NO_SHIFT:
                        // [规则 Q3-3.1.2] 有打卡但当天实际无排班，不计入任何异常统计
                        summary.no_shift_days++;
                        break;
                }
            }
        }
    }
}

// ------------------------------------------------------------------------------------------
// =================================== Excel工作表写入函数 ===================================
// ------------------------------------------------------------------------------------------

// ======================= 1. 导出考勤报表（全员）.xls =======================
bool ReportGenerator::exportAllAttendanceReport(const std::string& start_date, const std::string& end_date, const std::string& output_path) {
    long long start_ts = parseDateToTimestamp(start_date, false);
    long long end_ts = parseDateToTimestamp(end_date, true);
    if (start_ts == 0 || end_ts == 0) return false;

    // 1. 调用数据层接口获取全局基础数据
    std::vector<UserData> users = db_get_all_users_info(); // 获取所有员工及部门名
    std::vector<DeptInfo> depts = db_get_departments();    // 获取部门列表
    std::vector<ShiftInfo> shifts = db_get_shifts();       // 获取班次列表
    
    // 调用数据层接口：获取该时间段内全公司的所有考勤打卡记录
    std::vector<AttendanceRecord> records = db_get_records(start_ts, end_ts);

    // 2. 处理考勤核心数据 (复用您已有的数据清洗计算函数)
    std::map<int, std::map<int, DailyCellData>> detail_data;
    std::map<int, MonthlySummary> summaries;
    processAttendanceData(records, users, start_ts, end_ts, detail_data, summaries);

    // 筛选异常记录供“考勤异常统计表”使用
    std::vector<AttendanceRecord> abnormal_records;
    for (const auto& rec : records) {
        if (rec.status != STATUS_NORMAL) { // STATUS_NORMAL 为您原来的枚举值(0)
            abnormal_records.push_back(rec);
        }
    }

    // 3. 创建 Excel 工作簿
    lxw_workbook* workbook = workbook_new(output_path.c_str());
    if (!workbook) return false;

    // 4. 按文档要求依次生成 5 个 Sheet 标签页

    // --- 统一提取 year, month, days_in_month ---
    int year = 2024, month = 1, days_in_month = 31; 
    try {
        if (start_date.length() >= 7) {
            year = std::stoi(start_date.substr(0, 4));
            month = std::stoi(start_date.substr(5, 2));
            days_in_month = getDaysInMonth(year, month); // 获取当月天数
        }
    } catch (...) {
        std::cerr << "解析日期失败" << std::endl;
    }
    writeShiftInfoSheet(workbook, users, depts, shifts, start_date, end_date, year, month, days_in_month);
    writeSummarySheet(workbook, summaries, start_date, end_date);
    writeRecordSheet(workbook, records);
    writeAbnormalSheet(workbook, abnormal_records, start_date, end_date);
    writeDetailSheet(workbook, detail_data, summaries, start_date, end_date, year, month, days_in_month);

    // 5. 封存退出
    workbook_close(workbook);
    std::cout << "[Success] 考勤报表(全员)生成完毕: " << output_path << std::endl;
    return true;
}

// ======================= 2. 导出考勤报表（个人）.xls =======================
bool ReportGenerator::exportIndividualAttendanceReport(int user_id, const std::string& start_date, const std::string& end_date, const std::string& output_path) {
    long long start_ts = parseDateToTimestamp(start_date, false);
    long long end_ts = parseDateToTimestamp(end_date, true);
    if (start_ts == 0 || end_ts == 0) return false;

    // 1. 获取该个人的详情数据
    std::optional<UserData> user_opt = db_get_user_info(user_id);
    if (!user_opt.has_value()) {
        std::cerr << "[Error] 找不到该员工工号: " << user_id << std::endl;
        return false;
    }
    // 打包成 vector 以便兼容您的核心处理函数
    std::vector<UserData> users = { user_opt.value() }; 
    
    std::vector<DeptInfo> depts = db_get_departments();
    std::vector<ShiftInfo> shifts = db_get_shifts();

    // 真实接口调用：仅获取该工号(user_id)的考勤记录
    std::vector<AttendanceRecord> records = db_get_records_by_user(user_id, start_ts, end_ts);

    // --- 下方的处理与生成步骤与全员表完全相同 ---
    std::map<int, std::map<int, DailyCellData>> detail_data;
    std::map<int, MonthlySummary> summaries;
    processAttendanceData(records, users, start_ts, end_ts, detail_data, summaries);

    std::vector<AttendanceRecord> abnormal_records;
    for (const auto& rec : records) {
        if (rec.status != STATUS_NORMAL) {
            abnormal_records.push_back(rec);
        }
    }

    lxw_workbook* workbook = workbook_new(output_path.c_str());
    if (!workbook) return false;

    // --- 统一提取 year, month, days_in_month ---
    int year = 2024, month = 1, days_in_month = 31; 
    try {
        if (start_date.length() >= 7) {
            year = std::stoi(start_date.substr(0, 4));
            month = std::stoi(start_date.substr(5, 2));
            days_in_month = getDaysInMonth(year, month); // 获取当月天数
        }
    } catch (...) {
        std::cerr << "解析日期失败" << std::endl;
    }
    writeShiftInfoSheet(workbook, users, depts, shifts, start_date, end_date, year, month, days_in_month);
    writeSummarySheet(workbook, summaries, start_date, end_date);
    writeRecordSheet(workbook, records);
    writeAbnormalSheet(workbook, abnormal_records, start_date, end_date);
    writeDetailSheet(workbook, detail_data, summaries, start_date, end_date, year, month, days_in_month);

    workbook_close(workbook);
    std::cout << "[Success] 考勤报表(个人)生成完毕: " << output_path << std::endl;
    return true;
}

// ======================= 3. 导出 员工设置表.xls =======================
bool ReportGenerator::exportSettingsReport(const std::string& output_path) {
    // 1. 调用真实接口：获取导出设置表所需的所有外围数据
    std::vector<UserData> users = db_get_all_users_info();
    RuleConfig rules = db_get_global_rules();          // 获取机器防重复、韦根等配置规则
    std::vector<ShiftInfo> shifts = db_get_shifts();   // 获取最多10个时段的班次信息
    std::vector<DeptInfo> depts = db_get_departments();

    // 2. 创建工作簿
    lxw_workbook* workbook = workbook_new(output_path.c_str());
    if (!workbook) {
        std::cerr << "[Error] 创建设置表文件失败: " << output_path << std::endl;
        return false;
    }

    // 3. 按照文档生成两个 Sheet

    // --- 自动获取当前系统的年月 ---
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    int year = now->tm_year + 1900;
    int month = now->tm_mon + 1;
    int days_in_month = getDaysInMonth(year, month);
    
    // 生成当月 1 号的字符串作为排班日期参考
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%04d-%02d-01", year, month);
    std::string start_date = date_buf;

    writeEmployeeSettingsSheet(workbook, users, start_date, year, month, days_in_month);
    // 1. 获取真实数据
    RuleConfig config = db_get_global_rules();        // 获取规则配置
    //std::vector<DeptInfo> depts = db_get_departments();    // 获取部门 (请核对您的获取部门函数名)
    //std::vector<ShiftInfo> shifts = db_get_shifts(); // 获取班次

    // 2. 传入设置表
    writeAttendanceSettingsSheet(workbook, config, depts, shifts);

    workbook_close(workbook);
    std::cout << "[Success] 员工设置表(含设置)已生成: " << output_path << std::endl;
    return true;
}

//======================== 考勤报表.xls =======================

//排班信息表
void ReportGenerator::writeShiftInfoSheet(lxw_workbook* workbook, 
                                          const std::vector<UserData>& users, 
                                          const std::vector<DeptInfo>& depts,
                                          const std::vector<ShiftInfo>& shifts,
                                          const std::string& start_date,
                                          const std::string& end_date,
                                          int year, int month, int days_in_month) {
    // 1. 创建工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "排班信息表");

    // ================= 2. 准备格式刷 =================
    // 顶部日期和备注格式
    lxw_format *info_format = workbook_add_format(workbook);
    format_set_align(info_format, LXW_ALIGN_LEFT);
    format_set_align(info_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(info_format, LXW_BORDER_THIN);
    
    // 表头格式 (黄色背景)
    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0xFFFF00); // 纯黄色
    format_set_border(header_format, LXW_BORDER_THIN);
    format_set_align(header_format, LXW_ALIGN_CENTER);
    format_set_align(header_format, LXW_ALIGN_VERTICAL_CENTER);

    // 单元格格式
    lxw_format *cell_format = workbook_add_format(workbook);
    format_set_border(cell_format, LXW_BORDER_THIN);
    format_set_align(cell_format, LXW_ALIGN_CENTER);
    format_set_align(cell_format, LXW_ALIGN_VERTICAL_CENTER);

    // ================= 3. 设置列宽 =================
    worksheet_set_column(worksheet, 0, 0, 10, NULL); // 工号
    worksheet_set_column(worksheet, 1, 2, 12, NULL); // 姓名, 部门
    // 动态设置 1~31 号的列宽，稍窄一些
    worksheet_set_column(worksheet, 3, 3 + days_in_month - 1, 5, NULL);

    // ================= 4. 绘制表头 =================
    // 第0行：排班日期 和 备注说明
    std::string date_title = "排班日期：" + start_date + " ~ " + end_date;
    worksheet_merge_range(worksheet, 0, 0, 0, 2, date_title.c_str(), info_format);
    
    std::string remark_title = "备注：排班:1-10班次, 25-请假, 26-出差, 空/0-节假日;";
    worksheet_merge_range(worksheet, 0, 3, 0, 3 + days_in_month - 1, remark_title.c_str(), info_format);
    worksheet_set_row(worksheet, 0, 20, NULL);

    // 第1行和第2行：工号、姓名、部门 (跨行合并)
    worksheet_merge_range(worksheet, 1, 0, 2, 0, "工号", header_format);
    worksheet_merge_range(worksheet, 1, 1, 2, 1, "姓名", header_format);
    worksheet_merge_range(worksheet, 1, 2, 2, 2, "部门", header_format);

    // 第1行和第2行的右侧：填入 1~31 号 以及 星期几
    const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    for (int day = 1; day <= days_in_month; ++day) {
        int col = 2 + day;
        // 第1行写日期号 (1, 2, 3...)
        worksheet_write_number(worksheet, 1, col, day, header_format);

        // 计算这一天是星期几
        std::tm time_in = {0, 0, 0, day, month - 1, year - 1900};
        std::time_t time_temp = std::mktime(&time_in);
        const std::tm* time_out = std::localtime(&time_temp);
        
        // 第2行写星期几 (一, 二, 三...)
        worksheet_write_string(worksheet, 2, col, weekdays[time_out->tm_wday], header_format);
    }

    // 冻结前 3 行 和 前 3 列 (工号姓名部门滚动时固定)
    worksheet_freeze_panes(worksheet, 3, 3);

    // ================= 5. 写入数据 =================
    // 预处理部门字典，方便查找名称
    std::map<int, std::string> dept_map;
    for (const auto& d : depts) dept_map[d.id] = d.name;

    int row = 3;
    for (const auto& user : users) {
        std::string d_name = dept_map.count(user.dept_id) ? dept_map[user.dept_id] : "未知部门";

        // 写入工号、姓名、部门
        worksheet_write_number(worksheet, row, 0, user.id, cell_format);
        worksheet_write_string(worksheet, row, 1, user.name.c_str(), cell_format);
        worksheet_write_string(worksheet, row, 2, d_name.c_str(), cell_format);

        // 获取该员工当月的排班数据
        auto monthly_shifts = db_get_user_monthly_shifts(user.id, year, month);

        // 写入每天的排班班次
        for (int day = 1; day <= days_in_month; ++day) {
            int col = 2 + day;
            int shift_id = 0;
            if (monthly_shifts.count(day)) {
                shift_id = monthly_shifts[day].id;
            }

            if (shift_id > 0) {
                // 有班次则写入班次编号
                worksheet_write_number(worksheet, row, col, shift_id, cell_format);
            } else {
                // 没排班写入空或者0
                worksheet_write_string(worksheet, row, col, "", cell_format); 
            }
        }
        row++;
    }
}

//考勤汇总表
void ReportGenerator::writeSummarySheet(lxw_workbook* workbook, const std::map<int, MonthlySummary>& summaries, const std::string& start_date, const std::string& end_date) {
    // 1. 创建工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "考勤汇总表");

    // ================= 2. 精调格式刷 =================
    // 大标题格式
    lxw_format *title_format = workbook_add_format(workbook);
    format_set_bold(title_format);
    format_set_font_size(title_format, 16);
    format_set_align(title_format, LXW_ALIGN_CENTER);
    format_set_align(title_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(title_format, LXW_BORDER_NONE);

    // 第二行(部门/日期)格式
    lxw_format *left_format = workbook_add_format(workbook);
    format_set_align(left_format, LXW_ALIGN_LEFT);
    format_set_align(left_format, LXW_ALIGN_VERTICAL_CENTER);
    
    lxw_format *right_format = workbook_add_format(workbook);
    format_set_align(right_format, LXW_ALIGN_RIGHT);
    format_set_align(right_format, LXW_ALIGN_VERTICAL_CENTER);

    // 第三/四行：复杂表头格式 (加入自动换行以支持 \n)
    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0xFFFF00); // 纯黄色背景
    format_set_border(header_format, LXW_BORDER_THIN);
    format_set_align(header_format, LXW_ALIGN_CENTER);
    format_set_align(header_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_text_wrap(header_format); // 关键：允许“出勤天数\n(标准/实际)”换行

    // 普通数据单元格格式
    lxw_format *cell_format = workbook_add_format(workbook);
    format_set_border(cell_format, LXW_BORDER_THIN);
    format_set_align(cell_format, LXW_ALIGN_CENTER);
    format_set_align(cell_format, LXW_ALIGN_VERTICAL_CENTER);

    // ================= 3. 设置列宽 (共23列：0~22) =================
    worksheet_set_column(worksheet, 0, 0, 10, NULL); // 工号
    worksheet_set_column(worksheet, 1, 2, 12, NULL); // 姓名, 部门
    worksheet_set_column(worksheet, 3, 10, 8, NULL);  // 工时, 迟到早退加班等细节
    worksheet_set_column(worksheet, 11, 11, 14, NULL); // 出勤天数(标准/实际)
    worksheet_set_column(worksheet, 12, 21, 8, NULL); // 请假出差, 工资细节
    worksheet_set_column(worksheet, 22, 22, 15, NULL); // 备注

    // ================= 4. 绘制复杂表头 =================
    // 第0行：大标题 (跨 23 列)
    worksheet_merge_range(worksheet, 0, 0, 0, 22, "考勤汇总表", title_format);
    worksheet_set_row(worksheet, 0, 30, NULL);

    // 第1行：部门与日期
    worksheet_merge_range(worksheet, 1, 0, 1, 2, "部门: 所有部门", left_format);
    std::string date_str = "日期: " + start_date + " ~ " + end_date;
    worksheet_merge_range(worksheet, 1, 3, 1, 22, date_str.c_str(), right_format);
    worksheet_set_row(worksheet, 1, 20, NULL);

    // --- 第2行与第3行：绘制跨行(rowspan)和跨列(colspan)表头 ---
    // [跨行区] 纵向合并 第2行 和 第3行
    worksheet_merge_range(worksheet, 2, 0, 3, 0, "工号", header_format);
    worksheet_merge_range(worksheet, 2, 1, 3, 1, "姓名", header_format);
    worksheet_merge_range(worksheet, 2, 2, 3, 2, "部门", header_format);
    
    // [跨列区] 工作时数、迟到、早退、加班
    worksheet_merge_range(worksheet, 2, 3, 2, 4, "工作时数", header_format);
    worksheet_write_string(worksheet, 3, 3, "标准", header_format);
    worksheet_write_string(worksheet, 3, 4, "实际", header_format);

    worksheet_merge_range(worksheet, 2, 5, 2, 6, "迟到", header_format);
    worksheet_write_string(worksheet, 3, 5, "次数", header_format);
    worksheet_write_string(worksheet, 3, 6, "分钟", header_format);

    worksheet_merge_range(worksheet, 2, 7, 2, 8, "早退", header_format);
    worksheet_write_string(worksheet, 3, 7, "次数", header_format);
    worksheet_write_string(worksheet, 3, 8, "分钟", header_format);

    worksheet_merge_range(worksheet, 2, 9, 2, 10, "加班时数", header_format);
    worksheet_write_string(worksheet, 3, 9, "正常", header_format);
    worksheet_write_string(worksheet, 3, 10, "特殊", header_format);

    // [跨行区] 出勤天数、缺勤、请假、出差
    worksheet_merge_range(worksheet, 2, 11, 3, 11, "出勤天数\n(标准/实际)", header_format);
    worksheet_merge_range(worksheet, 2, 12, 3, 12, "缺勤", header_format);
    worksheet_merge_range(worksheet, 2, 13, 3, 13, "请假", header_format);
    worksheet_merge_range(worksheet, 2, 14, 3, 14, "出差", header_format);

    // [跨列区] 工资相关
    worksheet_merge_range(worksheet, 2, 15, 2, 17, "加项工资", header_format);
    worksheet_write_string(worksheet, 3, 15, "标注", header_format);
    worksheet_write_string(worksheet, 3, 16, "加班", header_format);
    worksheet_write_string(worksheet, 3, 17, "津贴", header_format);

    worksheet_merge_range(worksheet, 2, 18, 2, 20, "减项工资", header_format);
    worksheet_write_string(worksheet, 3, 18, "迟到/早退", header_format);
    worksheet_write_string(worksheet, 3, 19, "事假", header_format);
    worksheet_write_string(worksheet, 3, 20, "扣款", header_format);

    // [跨行区] 最终汇总
    worksheet_merge_range(worksheet, 2, 21, 3, 21, "实得工资", header_format);
    worksheet_merge_range(worksheet, 2, 22, 3, 22, "备注", header_format);

    // 冻结前4行（前4行一直可见）
    worksheet_freeze_panes(worksheet, 4, 0); 

    // ================= 5. 写入数据 =================
    int row = 4; // 数据从第4行开始写
    for (const auto& [user_id, summary] : summaries) {
        int actual_days = summary.normal_days + summary.late_count + summary.early_count;
        int expected_days = actual_days + summary.absent_days;

        // 写入工号、姓名、部门
        worksheet_write_string(worksheet, row, 0, summary.user_code.c_str(), cell_format);
        worksheet_write_string(worksheet, row, 1, summary.user_name.c_str(), cell_format);
        worksheet_write_string(worksheet, row, 2, summary.dept.c_str(), cell_format);

        // 工作时数 (暂时默认按每天8小时计算，您可以自行调整)
        worksheet_write_number(worksheet, row, 3, expected_days * 8, cell_format);
        worksheet_write_number(worksheet, row, 4, actual_days * 8, cell_format);

        // 迟到 (次数, 分钟)
        worksheet_write_number(worksheet, row, 5, summary.late_count, cell_format);
        worksheet_write_number(worksheet, row, 6, summary.total_late_minutes, cell_format);

        // 早退 (次数, 分钟)
        worksheet_write_number(worksheet, row, 7, summary.early_count, cell_format);
        worksheet_write_number(worksheet, row, 8, summary.total_early_minutes, cell_format);

        // 加班时数 (结构体中未提供，暂填0)
        worksheet_write_number(worksheet, row, 9, 0, cell_format);
        worksheet_write_number(worksheet, row, 10, 0, cell_format);

        // 出勤天数拼接文本 (例如 "22/21")
        std::string days_str = std::to_string(expected_days) + "/" + std::to_string(actual_days);
        worksheet_write_string(worksheet, row, 11, days_str.c_str(), cell_format);

        // 缺勤天数
        worksheet_write_number(worksheet, row, 12, summary.absent_days, cell_format);

        // 请假、出差、各类工资 (结构体暂无，填入0或留白待日后扩展)
        for (int c = 13; c <= 21; ++c) {
            worksheet_write_number(worksheet, row, c, 0, cell_format);
        }

        // 备注列
        worksheet_write_string(worksheet, row, 22, "", cell_format);

        row++;
    }
}

//考勤记录表
void ReportGenerator::writeRecordSheet(lxw_workbook* workbook, const std::vector<AttendanceRecord>& records) {
    // 1. 创建工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "考勤记录表");

    // 2. 准备格式刷
    lxw_format *title_format = workbook_add_format(workbook);
    format_set_bold(title_format);
    format_set_font_size(title_format, 16);
    format_set_align(title_format, LXW_ALIGN_CENTER);
    format_set_align(title_format, LXW_ALIGN_VERTICAL_CENTER);

    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0xBDD7EE); // 浅蓝色表头
    format_set_border(header_format, LXW_BORDER_THIN);
    format_set_align(header_format, LXW_ALIGN_CENTER);

    lxw_format *cell_format = workbook_add_format(workbook);
    format_set_border(cell_format, LXW_BORDER_THIN);
    format_set_align(cell_format, LXW_ALIGN_CENTER);

    lxw_format *red_format = workbook_add_format(workbook);
    format_set_border(red_format, LXW_BORDER_THIN);
    format_set_align(red_format, LXW_ALIGN_CENTER);
    format_set_font_color(red_format, LXW_COLOR_RED); // 异常状态标红

    // 3. 设置列宽
    worksheet_set_column(worksheet, 0, 0, 12, NULL); // 工号
    worksheet_set_column(worksheet, 1, 2, 15, NULL); // 姓名, 部门
    worksheet_set_column(worksheet, 3, 4, 15, NULL); // 打卡日期, 打卡时间
    worksheet_set_column(worksheet, 5, 6, 12, NULL); // 状态, 备注

    // 4. 画表头
    worksheet_merge_range(worksheet, 0, 0, 0, 6, "考勤记录明细表", title_format);
    const char* headers[] = {"工号", "姓名", "部门", "打卡日期", "打卡时间", "状态", "异常说明"};
    for (int i = 0; i < 7; ++i) {
        worksheet_write_string(worksheet, 1, i, headers[i], header_format);
    }
    worksheet_freeze_panes(worksheet, 2, 0); // 冻结前两行大标题和表头

    // 5. 遍历写入真实流水记录
    int row = 2;
    for (const auto& rec : records) {
        // 将时间戳拆分为日期和时间字符串
        std::time_t ts = rec.timestamp;
        std::tm* tm_info = std::localtime(&ts);
        char date_buf[32], time_buf[32];
        std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_info);
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

        // 状态文字及颜色判断
        std::string status_str = "正常";
        lxw_format* current_format = cell_format; // 默认黑色字体
        std::string remark = "";

        if (rec.status == STATUS_LATE) {
            status_str = "迟到";
            current_format = red_format;
            remark = std::to_string(rec.minutes_late) + " 分钟";
        } else if (rec.status == STATUS_EARLY) {
            status_str = "早退";
            current_format = red_format;
            remark = std::to_string(rec.minutes_early) + " 分钟";
        } else if (rec.status == STATUS_ABSENT) {
            status_str = "旷工";
            current_format = red_format;
        } else if (rec.status == STATUS_NO_SHIFT) {
            status_str = "未排班";
            // 未排班不视为严重异常，保持黑字
        }

        // 写入该行数据
        worksheet_write_number(worksheet, row, 0, rec.user_id, current_format);
        worksheet_write_string(worksheet, row, 1, rec.user_name.c_str(), current_format);
        worksheet_write_string(worksheet, row, 2, rec.dept_name.c_str(), current_format);
        worksheet_write_string(worksheet, row, 3, date_buf, current_format);
        worksheet_write_string(worksheet, row, 4, time_buf, current_format);
        worksheet_write_string(worksheet, row, 5, status_str.c_str(), current_format);
        worksheet_write_string(worksheet, row, 6, remark.c_str(), current_format);
        
        row++;
    }
}

//异常统计表
void ReportGenerator::writeAbnormalSheet(lxw_workbook* workbook, const std::vector<AttendanceRecord>& abnormal_records, const std::string& start_date, const std::string& end_date) {
    // 1. 创建名为“考勤异常统计表”的工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "考勤异常统计表");

    // ================= 2. 准备格式刷 =================
    // 大标题格式：加粗、大字体、居中
    lxw_format *title_format = workbook_add_format(workbook);
    format_set_bold(title_format);
    format_set_font_size(title_format, 16);
    format_set_align(title_format, LXW_ALIGN_CENTER);
    format_set_align(title_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(title_format, LXW_BORDER_THIN);

    // 第二行(考勤日期)格式
    lxw_format *date_label_format = workbook_add_format(workbook);
    format_set_align(date_label_format, LXW_ALIGN_CENTER);
    format_set_align(date_label_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(date_label_format, LXW_BORDER_THIN);

    lxw_format *date_value_format = workbook_add_format(workbook);
    format_set_align(date_value_format, LXW_ALIGN_LEFT);
    format_set_align(date_value_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(date_value_format, LXW_BORDER_THIN);

    // 表头格式：橙黄色背景、加粗、居中、带边框
    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0xFFC000); // 橙黄色以示异常
    format_set_border(header_format, LXW_BORDER_THIN);
    format_set_align(header_format, LXW_ALIGN_CENTER);
    format_set_align(header_format, LXW_ALIGN_VERTICAL_CENTER);

    // 普通数据单元格 / 异常标红单元格
    lxw_format *cell_format = workbook_add_format(workbook);
    format_set_border(cell_format, LXW_BORDER_THIN);
    format_set_align(cell_format, LXW_ALIGN_CENTER);
    format_set_align(cell_format, LXW_ALIGN_VERTICAL_CENTER);

    lxw_format *red_format = workbook_add_format(workbook);
    format_set_border(red_format, LXW_BORDER_THIN);
    format_set_align(red_format, LXW_ALIGN_CENTER);
    format_set_align(red_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_font_color(red_format, LXW_COLOR_RED);

    // ================= 3. 设置列宽 (共12列：0~11) =================
    worksheet_set_column(worksheet, 0, 0, 10, NULL); // 工号
    worksheet_set_column(worksheet, 1, 2, 12, NULL); // 姓名, 部门
    worksheet_set_column(worksheet, 3, 3, 14, NULL); // 日期
    worksheet_set_column(worksheet, 4, 7, 10, NULL); // 时段一、时段二(上下班)
    worksheet_set_column(worksheet, 8, 10, 10, NULL);// 迟到时间、早退时间、合计
    worksheet_set_column(worksheet, 11, 11, 15, NULL);// 备注

    // ================= 4. 绘制复杂表头 =================
    // 第0行：大标题 (合并 A1 到 L1)
    worksheet_merge_range(worksheet, 0, 0, 0, 11, "异常统计表", title_format);
    worksheet_set_row(worksheet, 0, 25, NULL);

    // 第1行：考勤日期说明
    worksheet_write_string(worksheet, 1, 0, "考勤日期", date_label_format);
    std::string date_str = start_date + " ~ " + end_date;
    worksheet_merge_range(worksheet, 1, 1, 1, 11, date_str.c_str(), date_value_format);
    worksheet_set_row(worksheet, 1, 20, NULL);

    // 第2行与第3行：绘制跨行(rowspan)和跨列(colspan)
    // [跨行区] 工号、姓名、部门、日期
    worksheet_merge_range(worksheet, 2, 0, 3, 0, "工号", header_format);
    worksheet_merge_range(worksheet, 2, 1, 3, 1, "姓名", header_format);
    worksheet_merge_range(worksheet, 2, 2, 3, 2, "部门", header_format);
    worksheet_merge_range(worksheet, 2, 3, 3, 3, "日期", header_format);

    // [跨列区] 时段一
    worksheet_merge_range(worksheet, 2, 4, 2, 5, "时段一", header_format);
    worksheet_write_string(worksheet, 3, 4, "上班", header_format);
    worksheet_write_string(worksheet, 3, 5, "下班", header_format);

    // [跨列区] 时段二
    worksheet_merge_range(worksheet, 2, 6, 2, 7, "时段二", header_format);
    worksheet_write_string(worksheet, 3, 6, "上班", header_format);
    worksheet_write_string(worksheet, 3, 7, "下班", header_format);

    // [跨行区] 迟到、早退、合计、备注
    worksheet_merge_range(worksheet, 2, 8, 3, 8, "迟到时间", header_format);
    worksheet_merge_range(worksheet, 2, 9, 3, 9, "早退时间", header_format);
    worksheet_merge_range(worksheet, 2, 10, 3, 10, "合计", header_format);
    worksheet_merge_range(worksheet, 2, 11, 3, 11, "备注", header_format);

    // 冻结前4行，向下滚动时表头固定
    worksheet_freeze_panes(worksheet, 4, 0);

    // ================= 5. 遍历写入数据 =================
    int row = 4; // 数据从第4行开始
    for (const auto& rec : abnormal_records) {
        // 时间戳转字符串
        std::time_t ts = rec.timestamp;
        std::tm* tm_info = std::localtime(&ts);
        char date_buf[32], time_buf[32];
        std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_info);
        std::strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info); // 只要 HH:MM

        // 核心逻辑：判断当前打卡记录到底属于什么异常
        int late = 0, early = 0;
        std::string t1_in = "", t1_out = "";
        std::string remark = "异常";

        if (rec.status == STATUS_LATE) {
            late = rec.minutes_late;
            t1_in = time_buf; // 迟到一般是上班打卡时间
            remark = "迟到";
        } else if (rec.status == STATUS_EARLY) {
            early = rec.minutes_early;
            t1_out = time_buf; // 早退一般是下班打卡时间
            remark = "早退";
        } else if (rec.status == STATUS_ABSENT) {
            remark = "旷工";
        }

        int total_abnormal = late + early;

        // 写入数据层真实字段 (AttendanceRecord)
        worksheet_write_number(worksheet, row, 0, rec.user_id, red_format);
        worksheet_write_string(worksheet, row, 1, rec.user_name.c_str(), red_format);
        worksheet_write_string(worksheet, row, 2, rec.dept_name.c_str(), red_format);
        worksheet_write_string(worksheet, row, 3, date_buf, red_format);
        
        // 时段一 (上下班)
        worksheet_write_string(worksheet, row, 4, t1_in.c_str(), red_format);
        worksheet_write_string(worksheet, row, 5, t1_out.c_str(), red_format);
        // 时段二 (目前数据层单次记录无时段二，留空预留)
        worksheet_write_string(worksheet, row, 6, "", red_format);
        worksheet_write_string(worksheet, row, 7, "", red_format);

        // 统计时长
        worksheet_write_number(worksheet, row, 8, late, red_format);
        worksheet_write_number(worksheet, row, 9, early, red_format);
        worksheet_write_number(worksheet, row, 10, total_abnormal, red_format);
        worksheet_write_string(worksheet, row, 11, remark.c_str(), red_format);
        
        row++;
    }
}

//考勤明细表
void ReportGenerator::writeDetailSheet(lxw_workbook* workbook, 
                                       const std::map<int, std::map<int, DailyCellData>>& detail_data, 
                                       const std::map<int, MonthlySummary>& summaries,
                                       const std::string& start_date, 
                                       const std::string& end_date,
                                       int year, int month, int days_in_month) {
    
    // 1. 创建工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "考勤明细表");

    // ================= 2. 准备格式刷 =================
    // 大标题
    lxw_format *title_format = workbook_add_format(workbook);
    format_set_bold(title_format);
    format_set_font_size(title_format, 16);
    format_set_align(title_format, LXW_ALIGN_CENTER);
    format_set_align(title_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(title_format, LXW_BORDER_THIN);

    // 日期标签和值
    lxw_format *date_label_format = workbook_add_format(workbook);
    format_set_align(date_label_format, LXW_ALIGN_CENTER);
    format_set_align(date_label_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(date_label_format, LXW_BORDER_THIN);

    lxw_format *date_value_format = workbook_add_format(workbook);
    format_set_align(date_value_format, LXW_ALIGN_LEFT);
    format_set_align(date_value_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(date_value_format, LXW_BORDER_THIN);

    // 表头格式：蓝绿色背景
    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0x92CDDC); // 蓝绿色，类似您图中的颜色
    format_set_border(header_format, LXW_BORDER_THIN);
    format_set_align(header_format, LXW_ALIGN_CENTER);
    format_set_align(header_format, LXW_ALIGN_VERTICAL_CENTER);

    // 数据单元格
    lxw_format *cell_format = workbook_add_format(workbook);
    format_set_border(cell_format, LXW_BORDER_THIN);
    format_set_align(cell_format, LXW_ALIGN_CENTER);
    format_set_align(cell_format, LXW_ALIGN_VERTICAL_CENTER);

    lxw_format *red_format = workbook_add_format(workbook);
    format_set_border(red_format, LXW_BORDER_THIN);
    format_set_align(red_format, LXW_ALIGN_CENTER);
    format_set_align(red_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_font_color(red_format, LXW_COLOR_RED);

    // ================= 3. 设置列宽 (共14列：0~13) =================
    worksheet_set_column(worksheet, 0, 0, 10, NULL); // 工号
    worksheet_set_column(worksheet, 1, 2, 12, NULL); // 姓名, 部门
    worksheet_set_column(worksheet, 3, 3, 14, NULL); // 日期
    worksheet_set_column(worksheet, 4, 4, 12, NULL); // 对应班次
    worksheet_set_column(worksheet, 5, 12, 16, NULL);// 各打卡时间与打卡结果
    worksheet_set_column(worksheet, 13, 13, 18, NULL);// 关联审批单

    // ================= 4. 绘制复杂表头 =================
    // 第0行：大标题 (合并 0 到 13 列)
    worksheet_merge_range(worksheet, 0, 0, 0, 13, "考勤明细表", title_format);
    worksheet_set_row(worksheet, 0, 25, NULL);

    // 第1行：考勤日期说明
    worksheet_write_string(worksheet, 1, 0, "考勤日期", date_label_format);
    std::string date_str = start_date + " ~ " + end_date;
    worksheet_merge_range(worksheet, 1, 1, 1, 13, date_str.c_str(), date_value_format);
    worksheet_set_row(worksheet, 1, 20, NULL);

    // 第2行：14列标准表头
    const char* headers[] = {
        "工号", "姓名", "部门", "日期", "对应班次",
        "上班1打卡时间", "上班1打卡结果", "下班1打卡时间", "下班1打卡结果",
        "上班2打卡时间", "上班2打卡结果", "下班2打卡时间", "下班2打卡结果",
        "关联的审批单"
    };
    for (int col = 0; col < 14; ++col) {
        worksheet_write_string(worksheet, 2, col, headers[col], header_format);
    }
    worksheet_set_row(worksheet, 2, 20, NULL);

    // 冻结前3行
    worksheet_freeze_panes(worksheet, 3, 0);

    // ================= 5. 遍历写入每日明细 =================
    int row = 3; // 数据从第3行开始
    char date_buf[32];

    // 遍历有考勤记录的所有员工
    for (const auto& [user_id, summary] : summaries) {
        // 取出该员工本月的每日打卡明细
        auto user_it = detail_data.find(user_id);
        if (user_it == detail_data.end()) continue;

        const auto& daily_records = user_it->second;

        // 遍历当月每一天
        for (int day = 1; day <= days_in_month; ++day) {
            auto day_it = daily_records.find(day);
            if (day_it == daily_records.end()) continue; // 当天无记录则跳过或视业务需求留白

            const DailyCellData& cell = day_it->second;

            // 构建日期字符串 (YYYY-MM-DD)
            snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", year, month, day);

            // 解析打卡结果 (如果您的 DailyCellData 有专门判断结果的逻辑，请在这里替换)
            std::string in_result = "正常";
            std::string out_result = "正常";
            lxw_format* current_format = cell_format;

            if (cell.status == STATUS_LATE) {
                in_result = "迟到";
                current_format = red_format;
            } else if (cell.status == STATUS_EARLY) {
                out_result = "早退";
                current_format = red_format;
            } else if (cell.status == STATUS_ABSENT) {
                in_result = "旷工";
                out_result = "旷工";
                current_format = red_format;
            } else if (cell.status == STATUS_NO_SHIFT) {
                in_result = "休息";
                out_result = "休息";
            }

            // 写入基础信息 (工号、姓名、部门、日期)
            worksheet_write_string(worksheet, row, 0, summary.user_code.c_str(), cell_format);
            worksheet_write_string(worksheet, row, 1, summary.user_name.c_str(), cell_format);
            worksheet_write_string(worksheet, row, 2, summary.dept.c_str(), cell_format);
            worksheet_write_string(worksheet, row, 3, date_buf, cell_format);

            // 对应班次 (如果 DailyCellData 有 shift_name，可直接替换)
            worksheet_write_string(worksheet, row, 4, "默认班次", cell_format);

            // 上班1 和 下班1 (使用您现有的 check_in 和 check_out)
            worksheet_write_string(worksheet, row, 5, cell.check_in.empty() ? "--:--" : cell.check_in.c_str(), current_format);
            worksheet_write_string(worksheet, row, 6, in_result.c_str(), current_format);
            worksheet_write_string(worksheet, row, 7, cell.check_out.empty() ? "--:--" : cell.check_out.c_str(), current_format);
            worksheet_write_string(worksheet, row, 8, out_result.c_str(), current_format);

            // 上下班2、审批单 (预留字段，目前填空)
            worksheet_write_string(worksheet, row, 9, "--:--", cell_format);
            worksheet_write_string(worksheet, row, 10, "--", cell_format);
            worksheet_write_string(worksheet, row, 11, "--:--", cell_format);
            worksheet_write_string(worksheet, row, 12, "--", cell_format);
            worksheet_write_string(worksheet, row, 13, "", cell_format);

            row++;
        }
    }
}

//======================== 员工设置表.xls =======================

//员工设置表
void ReportGenerator::writeEmployeeSettingsSheet(lxw_workbook* workbook, 
                                                 const std::vector<UserData>& users,
                                                 const std::string& start_date,
                                                 int year, int month, int days_in_month) {
    // 1. 创建工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "员工设置表");

    // ================= 2. 准备格式刷 =================
    // 橙色背景大标题居中
    lxw_format *orange_bg_center = workbook_add_format(workbook);
    format_set_bg_color(orange_bg_center, 0xF5CBA7); // 对应 HTML 的 #f5cba7
    format_set_align(orange_bg_center, LXW_ALIGN_CENTER);
    format_set_align(orange_bg_center, LXW_ALIGN_VERTICAL_CENTER);
    format_set_bold(orange_bg_center);
    format_set_border(orange_bg_center, LXW_BORDER_THIN);

    // 橙色背景备注居左且自动换行
    lxw_format *orange_bg_left = workbook_add_format(workbook);
    format_set_bg_color(orange_bg_left, 0xF5CBA7);
    format_set_align(orange_bg_left, LXW_ALIGN_LEFT);
    format_set_align(orange_bg_left, LXW_ALIGN_VERTICAL_CENTER);
    format_set_text_wrap(orange_bg_left); // 允许换行
    format_set_border(orange_bg_left, LXW_BORDER_THIN);

    // 表头格式：加粗居中
    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_align(header_format, LXW_ALIGN_CENTER);
    format_set_align(header_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(header_format, LXW_BORDER_THIN);

    // 普通单元格格式
    lxw_format *cell_format = workbook_add_format(workbook);
    format_set_align(cell_format, LXW_ALIGN_CENTER);
    format_set_align(cell_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(cell_format, LXW_BORDER_THIN);
    
    // 日期值靠左格式
    lxw_format *date_value_format = workbook_add_format(workbook);
    format_set_align(date_value_format, LXW_ALIGN_LEFT);
    format_set_align(date_value_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(date_value_format, LXW_BORDER_THIN);

    // ================= 3. 设置列宽 (共35列) =================
    worksheet_set_column(worksheet, 0, 1, 12, NULL); // 工号, 姓名
    worksheet_set_column(worksheet, 2, 2, 15, NULL); // 部门
    worksheet_set_column(worksheet, 3, 3, 10, NULL); // 权限
    worksheet_set_column(worksheet, 4, 34, 5, NULL); // 1-31日 (较窄)

    // ================= 4. 绘制复杂表头 =================
    // 第0行：员工设置表大标题 (跨 35 列)
    worksheet_merge_range(worksheet, 0, 0, 0, 34, "员工设置表", orange_bg_center);
    worksheet_set_row(worksheet, 0, 25, NULL);

    // 第1行：备注说明 (跨 35 列)
    std::string notes = "备注：\n"
                        "1. 上传员工信息请填写完整带*栏下的信息；\n"
                        "2. 编辑员工排班上传:1-10班次,25-请假,26-出差，空/0-节假日；\n"
                        "3. 权限:0-普通员工,1-管理员；";
    worksheet_merge_range(worksheet, 1, 0, 1, 34, notes.c_str(), orange_bg_left);
    worksheet_set_row(worksheet, 1, 65, NULL); // 调高行高以显示三行文本

    // 第2行：排班日期
    worksheet_merge_range(worksheet, 2, 0, 2, 3, "排班日期(例:2000-01-01)", header_format);
    worksheet_merge_range(worksheet, 2, 4, 2, 34, start_date.c_str(), date_value_format);
    worksheet_set_row(worksheet, 2, 20, NULL);

    // 第3/4行：跨行基础信息表头
    worksheet_merge_range(worksheet, 3, 0, 4, 0, "*工号", header_format);
    worksheet_merge_range(worksheet, 3, 1, 4, 1, "*姓名", header_format);
    worksheet_merge_range(worksheet, 3, 2, 4, 2, "*部门(1-16)", header_format);
    worksheet_merge_range(worksheet, 3, 3, 4, 3, "*权限", header_format);

    // 第3/4行：1~31号及其星期几
    const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    for (int day = 1; day <= 31; ++day) {
        int col = 3 + day;
        // 第3行写日期数字 1~31
        worksheet_write_number(worksheet, 3, col, day, header_format);

        // 如果该月有这一天，就算出星期几；否则留空
        if (day <= days_in_month) {
            std::tm time_in = {0, 0, 0, day, month - 1, year - 1900};
            std::time_t time_temp = std::mktime(&time_in);
            const std::tm* time_out = std::localtime(&time_temp);
            worksheet_write_string(worksheet, 4, col, weekdays[time_out->tm_wday], header_format);
        } else {
            worksheet_write_string(worksheet, 4, col, "", header_format);
        }
    }

    // 冻结前 5 行和前 4 列
    worksheet_freeze_panes(worksheet, 5, 4);

    // ================= 5. 写入数据 =================
    int row = 5;
    for (const auto& user : users) {
        worksheet_write_number(worksheet, row, 0, user.id, cell_format);
        worksheet_write_string(worksheet, row, 1, user.name.c_str(), cell_format);
        worksheet_write_number(worksheet, row, 2, user.dept_id, cell_format);
        worksheet_write_number(worksheet, row, 3, user.role, cell_format); // user.role 对应权限

        // 导出该员工现有的当月排班（如果有的话）
        auto monthly_shifts = db_get_user_monthly_shifts(user.id, year, month);
        for (int day = 1; day <= 31; ++day) {
            int col = 3 + day;
            if (day <= days_in_month) {
                int shift_id = (monthly_shifts.count(day)) ? monthly_shifts[day].id : 0;
                if (shift_id > 0) {
                    // 已有明确排班，直接写入
                    worksheet_write_number(worksheet, row, col, shift_id, cell_format);
                } else {
                    // 未设置排班：周一~周五默认班次1，周末为节假日（空）
                    std::tm time_in = {0, 0, 0, day, month - 1, year - 1900};
                    std::mktime(&time_in);
                    int wday = time_in.tm_wday; // 0=周日, 1=周一, ..., 6=周六
                    if (wday >= 1 && wday <= 5) {
                        worksheet_write_number(worksheet, row, col, 1, cell_format);
                    } else {
                        worksheet_write_string(worksheet, row, col, "", cell_format);
                    }
                }
            } else {
                worksheet_write_string(worksheet, row, col, "", cell_format);
            }
        }
        row++;
    }
}

//考勤设置表
void ReportGenerator::writeAttendanceSettingsSheet(lxw_workbook* workbook, 
                                                   const RuleConfig& config, 
                                                   const std::vector<DeptInfo>& depts, 
                                                   const std::vector<ShiftInfo>& shifts) {
    // 1. 创建工作表
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "考勤设置表");

    // 【核心亮点】：开启工作表保护。默认情况下所有单元格都会被锁定，无法修改。
    worksheet_protect(worksheet, "", NULL);

    // ================= 2. 准备格式刷 =================
    // 标题格式 (黄色不可修改)
    lxw_format *title_format = workbook_add_format(workbook);
    format_set_bg_color(title_format, 0xF5CBA7);
    format_set_bold(title_format);
    format_set_font_size(title_format, 16);
    format_set_align(title_format, LXW_ALIGN_CENTER);
    format_set_align(title_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(title_format, LXW_BORDER_THIN);

    // 表头与只读单元格格式 (黄色不可修改)
    lxw_format *readonly_format = workbook_add_format(workbook);
    format_set_bg_color(readonly_format, 0xF5CBA7);
    format_set_bold(readonly_format);
    format_set_align(readonly_format, LXW_ALIGN_CENTER);
    format_set_align(readonly_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(readonly_format, LXW_BORDER_THIN);

    // 可编辑单元格格式 (白色可修改)
    lxw_format *editable_format = workbook_add_format(workbook);
    format_set_bg_color(editable_format, 0xFFFFFF);
    format_set_align(editable_format, LXW_ALIGN_CENTER);
    format_set_align(editable_format, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(editable_format, LXW_BORDER_THIN);
    // 【关键】：单独为白色单元格解除锁定，这样用户就能在这里打字了！
    format_set_unlocked(editable_format); 

    // ================= 3. 设置列宽 (共16列：0~15) =================
    worksheet_set_column(worksheet, 0, 0, 8, NULL);  // 班次号
    worksheet_set_column(worksheet, 1, 6, 10, NULL); // 时段一、时段二、加班 (共6列)
    worksheet_set_column(worksheet, 7, 7, 8, NULL);  // 部门编号
    worksheet_set_column(worksheet, 8, 8, 15, NULL); // 部门名称
    worksheet_set_column(worksheet, 9, 15, 6, NULL); // 周日到周六排班 (共7列)

    // ================= 4. 绘制复杂表头 =================
    // 第0行：大标题
    worksheet_merge_range(worksheet, 0, 0, 0, 15, "考勤设置表", title_format);
    worksheet_set_row(worksheet, 0, 35, NULL);

    // 第1行：公司名称
    worksheet_merge_range(worksheet, 1, 0, 1, 1, "公司名称", readonly_format);
    worksheet_merge_range(worksheet, 1, 2, 1, 15, config.company_name.c_str(), editable_format);
    worksheet_set_row(worksheet, 1, 25, NULL);

    // 第2行：多级表头 (横向合并)
    worksheet_merge_range(worksheet, 2, 0, 3, 0, "班次号", readonly_format);
    worksheet_merge_range(worksheet, 2, 1, 2, 2, "时段一", readonly_format);
    worksheet_merge_range(worksheet, 2, 3, 2, 4, "时段二", readonly_format);
    worksheet_merge_range(worksheet, 2, 5, 2, 6, "加班", readonly_format);
    worksheet_merge_range(worksheet, 2, 7, 2, 8, "部门设置", readonly_format);
    worksheet_merge_range(worksheet, 2, 9, 2, 15, "部门排班 (1-10班次, 空/0-节假日)", readonly_format);
    worksheet_set_row(worksheet, 2, 20, NULL);

    // 第3行：子表头
    const char* sub_headers[] = {
        "上班", "下班", "上班", "下班", "上班", "下班", 
        "编号", "部门名称", "日", "一", "二", "三", "四", "五", "六"
    };
    for (int i = 0; i < 15; ++i) {
        worksheet_write_string(worksheet, 3, i + 1, sub_headers[i], readonly_format);
    }
    worksheet_set_row(worksheet, 3, 20, NULL);

    // 冻结前 4 行
    worksheet_freeze_panes(worksheet, 4, 0);

    // ================= 5. 填充数据区 (一共画 16 行：适配10个班次和16个部门) =================
    for (int i = 0; i < 16; ++i) {
        int row = 4 + i;
        
        // ------------- 左半部分：班次 (1-10) 或 考勤规则 (11-16) -------------
        if (i < 10) {
            // 写入班次号 1~10
            int shift_id = i + 1;
            worksheet_write_number(worksheet, row, 0, shift_id, readonly_format);
            
            // 写入班次时间
            if (static_cast<size_t>(i) < shifts.size()) {
                worksheet_write_string(worksheet, row, 1, shifts[i].s1_start.c_str(), editable_format);
                // 为了避免编译报错，底层未暴露的结构体字段暂填空字符串，HR可在模板里自行填写
                worksheet_write_string(worksheet, row, 2, "", editable_format); 
                worksheet_write_string(worksheet, row, 3, "", editable_format);
                worksheet_write_string(worksheet, row, 4, "", editable_format);
                worksheet_write_string(worksheet, row, 5, "", editable_format);
                worksheet_write_string(worksheet, row, 6, "", editable_format);
            } else {
                for(int c = 1; c <= 6; ++c) worksheet_write_string(worksheet, row, c, "", editable_format);
            }
        } else {
            // 利用左下角 (Row 14~19) 的空白区域，精准填入“考勤规则”
            if (i == 10) {
                worksheet_merge_range(worksheet, row, 0, row, 4, "上班晚多少分钟记迟到", readonly_format);
                worksheet_merge_range(worksheet, row, 5, row, 6, std::to_string(config.late_threshold).c_str(), editable_format);
            } else if (i == 11) {
                worksheet_merge_range(worksheet, row, 0, row, 4, "下班早多少分钟记早退", readonly_format);
                worksheet_merge_range(worksheet, row, 5, row, 6, "0", editable_format); 
            } else if (i == 12) {
                worksheet_merge_range(worksheet, row, 0, row, 4, "考勤重复确认时间(分钟)", readonly_format);
                worksheet_merge_range(worksheet, row, 5, row, 6, "3", editable_format);
            } else if (i == 13) {
                worksheet_merge_range(worksheet, row, 0, row, 4, "排班方式 (0-部门排班, 1-员工排班)", readonly_format);
                worksheet_merge_range(worksheet, row, 5, row, 6, "0", editable_format);
            } else {
                // 剩下两行合并为黄色空白墙，保持结构整齐
                worksheet_merge_range(worksheet, row, 0, row, 6, "", readonly_format);
            }
        }

        // ------------- 右半部分：部门设置与排班 (1-16) -------------
        int dept_id = i + 1;
        worksheet_write_number(worksheet, row, 7, dept_id, readonly_format);
        
        if (static_cast<size_t>(i) < depts.size()) {
            worksheet_write_string(worksheet, row, 8, depts[i].name.c_str(), editable_format);
            for(int d = 0; d < 7; ++d) {
                worksheet_write_string(worksheet, row, 9 + d, "1", editable_format); // 默认填班次1
            }
        } else {
            std::string default_name = "Not Set " + std::to_string(dept_id);
            worksheet_write_string(worksheet, row, 8, default_name.c_str(), editable_format);
            for(int d = 0; d < 7; ++d) {
                worksheet_write_string(worksheet, row, 9 + d, "1", editable_format);
            }
        }
    }
}

/**
 * @brief 获取用户指定月份的排班信息（新增批量接口）
 * @param user_id 用户ID
 * @param year 年份
 * @param month 月份
 * @return 用户在该月每天的排班信息
 */
std::map<int,ShiftInfo> ReportGenerator::db_get_user_monthly_shifts(int user_id, int year, int month) {
    std::map<int,ShiftInfo> monthly_shifts;

    int days_in_month = getDaysInMonth(year, month); // 获取当月天数

    // 循环获得每天的排班信息
    for(int day = 1; day <= days_in_month; day++){
        std::string date_str = formatDateString(year, month, day); // 构建日期字符串
        long long timestamp = parseDateToTimestamp(date_str, false);

        auto shift_opt = ::db_get_user_shift_smart(user_id, timestamp); 
        
        if (shift_opt.has_value()) {
            monthly_shifts[day] = shift_opt.value(); // 取出实际排班信息
        } else {
            ShiftInfo empty_shift; 
            empty_shift.id = 0; // 如果没排班/休息，返回一个 id=0 的空对象存入 map
            monthly_shifts[day] = empty_shift;
        }
    }
    
    return monthly_shifts;
}

/**
 * @brief 将时间格式化为字符串
 * @param year 年份
 * @param month 月份  
 * @param day 日期
 * @return 格式化字符串 "YYYY-MM-DD"
 */
std::string ReportGenerator::formatDateString(int year, int month, int day){
    std::stringstream ss;
    ss << year << "-"
       <<std::setw(2) << std::setfill('0') << month << "-"
       <<std::setw(2) << std::setfill('0') <<day;
    return ss.str();
}



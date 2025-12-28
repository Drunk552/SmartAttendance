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

/**
 * @brief 将时间字符串 "HH:MM" 转换为分钟数
 * @param time_str 时间字符串
 * @return 分钟数
 */
int timeStrToMinutes(const std::string& time_str) {
    if (time_str.empty()) return -1; // 无效时间
    int h, m;
    char sep;
    std::stringstream ss(time_str);
    ss >> h >> sep >> m;
    return h * 60 + m;
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
        rec.id = record.id; // 保留原ID
        rec.user_id = record.user_id; // 保留原用户ID
        rec.timestamp = record.timestamp; // 保留原时间戳
        rec.status = record.status; // 保留原状态
        rec.user_name = record.user_name; // 保留原用户名
        rec.dept_name = record.dept_name; // 保留原部门名称
        rec.image_path = record.image_path;

        rec.minutes_late = 0; // 默认值为0
        rec.minutes_early = 0; // 默认值为0

        // 动态获取班次信息
        // 生成缓存 Key (用户ID + 日期)
        std::string date_str = formatDate(rec.timestamp);
        std::string cache_key = std::to_string(rec.user_id) + "_" + date_str;

        ShiftInfo current_shift;
        // 检查缓存或查询数据库
        if (shift_cache.find(cache_key) != shift_cache.end()) {
            current_shift = shift_cache[cache_key];
        } 
        
        else {
            //智能获取当天的排班 (个人排班 > 部门排班 > 默认班次)
            current_shift = ::db_get_user_shift_smart(rec.user_id, rec.timestamp);
            shift_cache[cache_key] = current_shift;
        } 
        
        //计算迟到/早退逻辑
        int late_min = calculateLateMinutes(rec.timestamp, current_shift);
        int early_min = calculateEarlyMinutes(rec.timestamp, current_shift);

        //根据计算结果强制更新状态和分钟数
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
            return LXW_COLOR_RED; // Epic5.3: 红色字体
        case STATUS_EARLY:
            return 0xFF9900; // 橙色
        case STATUS_NORMAL:
            return 0x00A933; // 绿色
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
    
    // 处理考勤记录
    for (const auto& rec : records) {
        int day = extractDayFromTimestamp(rec.timestamp);
        if (day == 0) continue;
        
        DailyCellData& cell_data = detail_data[rec.user_id][day];
        
        if (cell_data.user_name.empty()) {
            cell_data.user_name = rec.user_name;
            cell_data.user_code = std::to_string(rec.user_id);
            cell_data.check_in = "--:--";
            cell_data.check_out = "--:--";
            cell_data.status = STATUS_NORMAL;
        }
        
        std::string time_str = formatTime(rec.timestamp);
        
        if (cell_data.check_in == "--:--") {
            cell_data.check_in = time_str;
            cell_data.status = rec.status;
            cell_data.late_minutes = rec.minutes_late;
        } else if (time_str > cell_data.check_in && cell_data.check_out == "--:--") {
            cell_data.check_out = time_str;
            if (rec.status == STATUS_EARLY) {
                cell_data.status = STATUS_EARLY;
                cell_data.early_minutes = rec.minutes_early;
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
                DailyCellData absent_data;
                absent_data.user_name = summary.user_name;
                absent_data.user_code = summary.user_code;
                absent_data.check_in = "--:--";
                absent_data.check_out = "--:--";
                absent_data.status = STATUS_ABSENT; //缺勤标记
                user_data[day] = absent_data;
                
                summary.absent_days++;
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
                }
            }
        }
    }
}

// ==================== Excel工作表写入函数 ====================

/**
 * @brief 写入汇总报表工作表
 * @param workbook 工作簿对象
 * @param summaries 月度汇总数据
 */
void ReportGenerator::writeSummarySheet(lxw_workbook* workbook, 
                                      const std::map<int, MonthlySummary>& summaries) {
    lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Summary");
    
    // 创建样式
    lxw_format* header_format = createHeaderFormat(workbook);
    lxw_format* normal_format = createNormalFormat(workbook);
    lxw_format* red_format = createRedFormat(workbook);
    lxw_format* green_format = createGreenFormat(workbook);
    
    // 写入表头
    const char* headers[] = {"姓名", "工号", "部门", "正常天数", "迟到次数", 
                           "迟到分钟", "早退次数", "早退分钟", "缺勤天数", "备注"};
    
    for (int i = 0; i < 10; i++) {
        worksheet_write_string(sheet, 0, i, headers[i], header_format);
    }
    
    // 写入数据
    int row = 1;
    for (const auto& [user_id, summary] : summaries) {
        worksheet_write_string(sheet, row, 0, summary.user_name.c_str(), normal_format);
        worksheet_write_string(sheet, row, 1, summary.user_code.c_str(), normal_format);
        worksheet_write_string(sheet, row, 2, summary.dept.c_str(), normal_format);
        worksheet_write_number(sheet, row, 3, summary.normal_days, green_format);
        worksheet_write_number(sheet, row, 4, summary.late_count, red_format);
        worksheet_write_number(sheet, row, 5, summary.total_late_minutes, red_format);
        worksheet_write_number(sheet, row, 6, summary.early_count, red_format);
        worksheet_write_number(sheet, row, 7, summary.total_early_minutes, red_format);
        worksheet_write_number(sheet, row, 8, summary.absent_days, red_format);
        
        // 备注
        std::string remark;
        if (summary.late_count > 0) {
            remark += "迟到" + std::to_string(summary.late_count) + "次";
            if (summary.total_late_minutes > 0) {
                remark += "(" + std::to_string(summary.total_late_minutes) + "分钟) ";
            }
        }
        if (summary.early_count > 0) {
            remark += "早退" + std::to_string(summary.early_count) + "次";
            if (summary.total_early_minutes > 0) {
                remark += "(" + std::to_string(summary.total_early_minutes) + "分钟) ";
            }
        }
        if (summary.absent_days > 0) {
            remark += "缺勤" + std::to_string(summary.absent_days) + "天";
        }
        if (remark.empty()) remark = "全勤";
        
        worksheet_write_string(sheet, row, 9, remark.c_str(), normal_format);
        row++;
    }
    
    // 设置列宽
    worksheet_set_column(sheet, 0, 0, 15, NULL);
    worksheet_set_column(sheet, 1, 1, 12, NULL);
    worksheet_set_column(sheet, 2, 2, 12, NULL);
    worksheet_set_column(sheet, 3, 3, 12, NULL);
    worksheet_set_column(sheet, 4, 5, 12, NULL);
    worksheet_set_column(sheet, 6, 7, 12, NULL);
    worksheet_set_column(sheet, 8, 8, 12, NULL);
    worksheet_set_column(sheet, 9, 9, 25, NULL);
}

/**
 * @brief 写入每日明细工作表
 * @param workbook 工作簿对象
 * @param detail_data 每日明细数据
 * @param days_in_month 当月天数
 */
void ReportGenerator::writeDetailSheet(lxw_workbook* workbook,
                                     const std::map<int, std::map<int, DailyCellData>>& detail_data,
                                     int days_in_month) {
    lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Daily Detail");
    
    // 创建样式
    lxw_format* header_format = createHeaderFormat(workbook);
    lxw_format* normal_format = createNormalFormat(workbook);
    lxw_format* red_format = createRedFormat(workbook);
    lxw_format* green_format = createGreenFormat(workbook);
    lxw_format* yellow_format = createYellowFormat(workbook);
    
    // 写入表头
    worksheet_write_string(sheet, 0, 0, "姓名/工号", header_format);
    
    // Epic5.3: 横轴为日期，写入1-31日
    for (int day = 1; day <= days_in_month; day++) {
        worksheet_write_number(sheet, 0, day, day, header_format);
    }
    
    // 设置列宽
    worksheet_set_column(sheet, 0, 0, 20, NULL); // 姓名列
    for (int day = 1; day <= days_in_month; day++) {
        worksheet_set_column(sheet, day, day, 10, NULL); // 日期列
    }
    
    // 写入数据
    int row = 1;
    for (const auto& [user_id, user_data] : detail_data) {
        // Epic5.3: 纵轴为员工，第一列为"姓名/工号"
        std::string name_id;
        if (!user_data.empty()) {
            const DailyCellData& first_cell = user_data.begin()->second;
            name_id = first_cell.user_name + "/" + first_cell.user_code;
        }
        worksheet_write_string(sheet, row, 0, name_id.c_str(), normal_format);
        
        // 写入每日考勤状态
        for (int day = 1; day <= days_in_month; day++) {
            if (user_data.find(day) != user_data.end()) {
                const DailyCellData& cell_data = user_data.at(day);
                lxw_format* cell_format = normal_format;
                std::string display_text;
                
                switch (cell_data.status) {
                    case STATUS_ABSENT: // Epic5.3: 缺勤显示A，红色字体
                        display_text = "A";
                        cell_format = red_format;
                        break;
                        
                    case STATUS_LATE: // Epic5.3: 迟到显示L或时间，红色字体
                        if (cell_data.check_in != "--:--") {
                            display_text = "L(" + cell_data.check_in + ")";
                        } else {
                            display_text = "L";
                        }
                        cell_format = red_format; // Epic5.3: 字体颜色为红色
                        break;
                        
                    case STATUS_EARLY: // 早退
                        if (cell_data.check_out != "--:--") {
                            display_text = "✓(" + cell_data.check_out + ")";
                        } else {
                            display_text = "✓";
                        }
                        cell_format = yellow_format;
                        break;
                        
                    case STATUS_NORMAL: // Epic5.3: 正常打卡显示✓或具体时间
                        if (cell_data.check_in != "--:--") {
                            display_text = cell_data.check_in;
                        } else {
                            display_text = "✓";
                        }
                        cell_format = green_format;
                        break;
                }
                
                worksheet_write_string(sheet, row, day, display_text.c_str(), cell_format);
            }
        }
        row++;
    }
    
    // 冻结窗格
    worksheet_freeze_panes(sheet, 1, 1);
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

        ShiftInfo shift = ::db_get_user_shift_smart(user_id, timestamp); // 调用现有的智能获取排班接口
        
        monthly_shifts[day] = shift;
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

/**
 * @brief 写入排班信息表
 * @param workbook 工作簿对象
 * @param users 用户信息列表
 * @param year 年份
 * @param month 月份
 */
void ReportGenerator::writeShiftSheet(lxw_workbook* workbook, const std::vector<UserData>& users, int year, int month)  {
    lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Shift Information");

    // 创建样式
    lxw_format* header_format = createHeaderFormat(workbook);
    lxw_format* normal_format = createNormalFormat(workbook);
    lxw_format* rest_format = workbook_add_format(workbook); // 休息日样式
    format_set_font_color(rest_format, 0x808080); // 灰色
    format_set_border(rest_format, LXW_BORDER_THIN);
    format_set_align(rest_format, LXW_ALIGN_CENTER);

    // 获取当月天数
    int days_in_month = getDaysInMonth(year, month);

    // 写入表头
    worksheet_write_string(sheet, 0, 0, "员工/日期", header_format);

    // 写入日期行(横轴：1-31日)
    for(int day = 1; day <= days_in_month; day++){
        worksheet_write_number(sheet, 0, day, day, header_format);
    }

    // 写入员工排班数据(纵轴：员工)
    int row = 1;
    for(const auto& user : users){
        // 写入员工信息（姓名/工号）
        std::string user_info = user.name + "/" + std::to_string(user.id);
        worksheet_write_string(sheet, row, 0, user_info.c_str(), normal_format);

        // 获取该员工当月排班信息
        std::map<int, ShiftInfo> monthly_shifts = db_get_user_monthly_shifts(user.id, year, month);

        // 写入每天排班的信息
        for(int day = 1; day <= days_in_month; day++){
            std::string shift_display = "休"; // 默认休息
        
            if(monthly_shifts.find(day) != monthly_shifts.end()){
                const ShiftInfo& shift = monthly_shifts[day];

                // 判断是否为休息日 - 根据 db_get_user_shift_smart 逻辑
                // 如果没有排班或排班ID为0，视为休息
                if(shift.id == 0) {
                    shift_display = "休";
                } else {
                    // 根据 db_storage.h 中的字段名访问
                    if (!shift.name.empty()) {
                        // 如果有班次名称，显示班次名称
                        shift_display = shift.name;
                    } else if (!shift.s1_start.empty() && !shift.s1_end.empty()) {
                        // 如果没有班次名称，显示时间段
                        shift_display = shift.s1_start.substr(0, 5) + "-" + shift.s1_end.substr(0, 5);
                        
                        // 如果有多个班段，可以添加更多信息
                        if (!shift.s2_start.empty() && !shift.s2_end.empty()) {
                            shift_display += " " + shift.s2_start.substr(0, 5) + "-" + shift.s2_end.substr(0, 5);
                        }
                    } else {
                        // 默认显示
                        shift_display = "班";
                    }
                }
                
                // 写入单元格
                lxw_format* cell_format = (shift_display == "休") ? rest_format : normal_format; 
                worksheet_write_string(sheet, row, day, shift_display.c_str(), cell_format);
            } else {
                // 如果没有排班数据，默认显示休息
                worksheet_write_string(sheet, row, day, "休", rest_format);
            }
        }

        row++;
    }

    // 设置列宽
    worksheet_set_column(sheet, 0, 0, 20, NULL); // 员工列
    for(int day = 1; day <= days_in_month; day++){
        worksheet_set_column(sheet, day, day, 12, NULL); // 日期列
    }

    // 冻结窗格
    worksheet_freeze_panes(sheet, 1, 1);

    std::cout << "[Report] 排班信息表创建完成 (" << row-1 << "名员工, " << days_in_month << "天)" << std::endl;
}

/**
 * @brief 写入原始记录表
 * @param workbook 工作簿对象
 * @param records 考勤记录列表
 */
void ReportGenerator::writeRecordSheet(lxw_workbook* workbook, const std::vector<AttendanceRecord>& records) {
    lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Original Records");
    
    // 创建样式
    lxw_format* header_format = createHeaderFormat(workbook);
    lxw_format* normal_format = createNormalFormat(workbook);
    lxw_format* time_format = workbook_add_format(workbook);
    format_set_border(time_format, LXW_BORDER_THIN);
    format_set_align(time_format, LXW_ALIGN_CENTER);
    format_set_num_format(time_format, "yyyy-mm-dd hh:mm:ss");
    
    // 写入表头
    const char* headers[] = {
        "工号", "姓名", "部门", "打卡日期", "打卡时间", "设备ID", "验证方式"
    };
    
    for (int i = 0; i < 7; i++) {
        worksheet_write_string(sheet, 0, i, headers[i], header_format);
    }
    
    // 写入数据
    int row = 1;
    for (const auto& record : records) {
        // 工号
        worksheet_write_number(sheet, row, 0, record.user_id, normal_format);
        
        // 姓名
        worksheet_write_string(sheet, row, 1, record.user_name.c_str(), normal_format);
        
        // 部门
        worksheet_write_string(sheet, row, 2, record.dept_name.c_str(), normal_format);
        
        // 打卡日期和时间
        std::time_t t = (std::time_t)record.timestamp;
        std::tm* tm = std::localtime(&t);
        
        if (tm) {
            // 打卡日期
            char date_buf[16];
            std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm);
            worksheet_write_string(sheet, row, 3, date_buf, normal_format);
            
            // 打卡时间（精确到秒）
            char time_buf[16];
            std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);
            worksheet_write_string(sheet, row, 4, time_buf, normal_format);
            
            // 如果需要Excel日期时间格式
            // double excel_time = ((double)record.timestamp / 86400.0) + 25569.0;
            // worksheet_write_datetime(sheet, row, 4, &excel_time, time_format);
        } else {
            worksheet_write_string(sheet, row, 3, "N/A", normal_format);
            worksheet_write_string(sheet, row, 4, "N/A", normal_format);
        }
        
        // 设备ID（可选）
        std::string device_id = "N/A"; // 假设没有设备ID字段
        // 如果有设备ID字段：device_id = record.device_id;
        worksheet_write_string(sheet, row, 5, device_id.c_str(), normal_format);
        
        // 验证方式
        std::string verify_mode = "人脸"; // 默认为人脸识别
        // 可以根据实际情况判断
        // if (!record.card_id.empty()) verify_mode = "刷卡";
        worksheet_write_string(sheet, row, 6, verify_mode.c_str(), normal_format);
        
        row++;
    }
    
    // 设置列宽
    worksheet_set_column(sheet, 0, 0, 10, NULL); // 工号
    worksheet_set_column(sheet, 1, 1, 15, NULL); // 姓名
    worksheet_set_column(sheet, 2, 2, 15, NULL); // 部门
    worksheet_set_column(sheet, 3, 3, 12, NULL); // 日期
    worksheet_set_column(sheet, 4, 4, 12, NULL); // 时间
    worksheet_set_column(sheet, 5, 5, 15, NULL); // 设备ID
    worksheet_set_column(sheet, 6, 6, 12, NULL); // 验证方式
    
    std::cout << "[Report] 原始记录表创建完成 (" << row-1 << " 条记录)" << std::endl;
}

/**
 * @brief 写入异常统计表
 * @param workbook 工作簿对象
 * @param records 考勤记录列表
 * @param summaries 月度汇总数据
 * @param detail_data 每日明细数据（用于获取具体缺勤日期）
 * @param year 年份
 * @param month 月份
 */
void ReportGenerator::writeExceptionSheet(lxw_workbook* workbook, 
                                         const std::vector<AttendanceRecord>& records,
                                         const std::map<int, MonthlySummary>& summaries,
                                         const std::map<int, std::map<int, DailyCellData>>& detail_data,
                                         int year, int month) {
    
    lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Exceptions");
    
    // 创建样式
    lxw_format* header_format = createHeaderFormat(workbook);
    lxw_format* normal_format = createNormalFormat(workbook);
    lxw_format* late_format = workbook_add_format(workbook); // 迟到样式
    format_set_font_color(late_format, 0xFF6600); // 橙色
    format_set_border(late_format, LXW_BORDER_THIN);
    format_set_align(late_format, LXW_ALIGN_CENTER);
    
    lxw_format* early_format = workbook_add_format(workbook); // 早退样式
    format_set_font_color(early_format, 0x9933CC); // 紫色
    format_set_border(early_format, LXW_BORDER_THIN);
    format_set_align(early_format, LXW_ALIGN_CENTER);
    
    lxw_format* absent_format = createRedFormat(workbook); // 缺勤样式（红色）
    
    // 写入表头
    const char* headers[] = {
        "工号", "姓名", "部门", "异常日期", "异常类型", "异常详情"
    };
    
    for (int i = 0; i < 6; i++) {
        worksheet_write_string(sheet, 0, i, headers[i], header_format);
    }
    
    // 收集异常记录
    std::vector<ExceptionRecord> exception_records;
    
    // 从原始记录中提取迟到和早退
    for (const auto& record : records) {

        if (record.status != STATUS_NORMAL) {
            ExceptionRecord exc;
            exc.user_id = record.user_id;
            exc.user_name = record.user_name;
            exc.dept_name = record.dept_name;
            exc.date = formatDate(record.timestamp);
            
            // 确定异常类型和详情
            if (record.status == STATUS_LATE) {
                exc.exception_type = "迟到";
                exc.exception_detail = "迟到 " + std::to_string(record.minutes_late) + "分钟";
            } 
            
            else if (record.status == STATUS_EARLY) {
                exc.exception_type = "早退";
                exc.exception_detail = "早退 " + std::to_string(record.minutes_early) + "分钟";
            } 
            
            else if (record.status == STATUS_ABSENT) {
                exc.exception_type = "缺勤";
                exc.exception_detail = "全天缺勤";
            }
            
            else {
                exc.exception_type = "其他异常";
                exc.exception_detail = "状态码: " + std::to_string(record.status);
            }
            
            exception_records.push_back(exc);
        }
    }
    
    // 从detail_data中提取具体的缺勤日期
    for (const auto& [user_id, daily_data] : detail_data) {
        // 获取用户信息
        std::string user_name, dept_name;

        if (daily_data.size() > 0) {
            user_name = daily_data.begin()->second.user_name;
           
            // 从汇总数据中获取部门信息
            if (summaries.find(user_id) != summaries.end()) {
                dept_name = summaries.at(user_id).dept;
            }
        }
        
        // 遍历每一天，找出缺勤日期
        for (const auto& [day, cell_data] : daily_data) {

            if (cell_data.status == STATUS_ABSENT) {
                ExceptionRecord exc;
                exc.user_id = user_id;
                exc.user_name = user_name;
                exc.dept_name = dept_name;
                
                // 构建具体日期
                exc.date = std::to_string(year) + "-" + 
                          std::to_string(month) + "-" + 
                          std::to_string(day);
                exc.exception_type = "缺勤";
                exc.exception_detail = "全天未打卡";
                
                exception_records.push_back(exc);
            }
        }
    }
    
    // 写入异常记录数据
    int row = 1;
    for (const auto& exc : exception_records) {
        // 工号
        worksheet_write_number(sheet, row, 0, exc.user_id, normal_format);
        
        // 姓名
        worksheet_write_string(sheet, row, 1, exc.user_name.c_str(), normal_format);
        
        // 部门
        worksheet_write_string(sheet, row, 2, exc.dept_name.c_str(), normal_format);
        
        // 异常日期
        worksheet_write_string(sheet, row, 3, exc.date.c_str(), normal_format);
        
        // 异常类型（根据类型使用不同样式）
        lxw_format* type_format = normal_format;
        
        if (exc.exception_type == "迟到") {
            type_format = late_format;
        } 
        
        else if (exc.exception_type == "早退") {
            type_format = early_format;
        } 
        
        else if (exc.exception_type == "缺勤") {
            type_format = absent_format;
        }
        worksheet_write_string(sheet, row, 4, exc.exception_type.c_str(), type_format);
        
        // 异常详情
        worksheet_write_string(sheet, row, 5, exc.exception_detail.c_str(), type_format);
        
        row++;
    }
    
    // 设置列宽
    worksheet_set_column(sheet, 0, 0, 10, NULL); // 工号
    worksheet_set_column(sheet, 1, 1, 15, NULL); // 姓名
    worksheet_set_column(sheet, 2, 2, 15, NULL); // 部门
    worksheet_set_column(sheet, 3, 3, 12, NULL); // 异常日期
    worksheet_set_column(sheet, 4, 4, 10, NULL); // 异常类型
    worksheet_set_column(sheet, 5, 5, 20, NULL); // 异常详情
    
    std::cout << "[Report] 异常统计表创建完成 (" << row-1 << " 条异常记录)" << std::endl;
}

/**
 * @brief 导出自定义时间范围的详细报表
 * @param start_date 起始日期字符串 (格式: "YYYY-MM-DD")
 * @param end_date 结束日期字符串 (格式: "YYYY-MM-DD")
 * @param user_id_filter 用户ID过滤 (-1表示所有用户)
 * @param output_path 输出文件路径
 * @return 成功返回true，失败返回false
 */
bool ReportGenerator::exportCustomRangeDetailedReport(const std::string& start_date, 
                                                      const std::string& end_date, 
                                                      int user_id_filter, 
                                                      const std::string& output_path) {
    std::cout << "[Report] 导出自定义报表: " << start_date << " ~ " << end_date 
              << " (UserFilter: " << user_id_filter << ")" << std::endl;

    // 1. 解析时间
    long long start_ts = parseDateToTimestamp(start_date, false);
    long long end_ts = parseDateToTimestamp(end_date, true);
    
    // 计算总天数 (用于生成明细表的列数)
    int total_days = (end_ts - start_ts) / 86400 + 1;
    if (total_days <= 0) total_days = 1;

    // 2. 获取原始数据
    std::vector<AttendanceRecord> records = db_get_records(start_ts, end_ts);
    std::vector<UserData> all_users = db_get_all_users_info();
    std::vector<UserData> target_users;

    // 3. 筛选目标用户
    if (user_id_filter != -1) {
        bool found = false;
        for (const auto& u : all_users) {
            if (u.id == user_id_filter) {
                target_users.push_back(u);
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "[Error] 未找到工号: " << user_id_filter << std::endl;
            return false;
        }
    } else {
        target_users = all_users;
    }

    // 4. 构建数据结构 (模仿 processAttendanceData 但适配自定义天数)
    std::map<int, std::map<int, DailyCellData>> detail_data;
    std::map<int, MonthlySummary> summaries;

    // 初始化
    for (const auto& user : target_users) {
        detail_data[user.id] = std::map<int, DailyCellData>();
        MonthlySummary summary;
        summary.user_name = user.name;
        summary.user_code = std::to_string(user.id);
        summary.dept = user.dept_name;
        summaries[user.id] = summary;
    }

    // 处理记录，将日期映射为索引 (第1天, 第2天...)
    for (const auto& rec : records) {
        if (user_id_filter != -1 && rec.user_id != user_id_filter) continue;

        // 计算这是第几天 (1-based index)
        long long rec_day_start = parseDateToTimestamp(formatDate(rec.timestamp), false);
        int day_idx = (rec_day_start - start_ts) / 86400 + 1;
        
        if (day_idx < 1 || day_idx > total_days) continue;

        DailyCellData& cell = detail_data[rec.user_id][day_idx]; // <--- 这里定义的变量名是 cell
        
        // 初始化单元格
        if (cell.user_name.empty()) {
            cell.user_name = rec.user_name;
            cell.user_code = std::to_string(rec.user_id);
            cell.check_in = "--:--";
            cell.check_out = "--:--";
            cell.status = STATUS_NORMAL;
        }

        std::string time_str = formatTime(rec.timestamp);
        
        // 简单的打卡逻辑复用
        if (cell.check_in == "--:--") {
            cell.check_in = time_str;
            cell.status = rec.status;
            cell.late_minutes = rec.minutes_late;
        } 
        else if (time_str > cell.check_in && cell.check_out == "--:--") {
            cell.check_out = time_str;
            if (rec.status == STATUS_EARLY) {
                cell.status = STATUS_EARLY;
                cell.early_minutes = rec.minutes_early;
            }
        }
    }

    // 补全缺勤并统计
    for (auto& [user_id, user_data] : detail_data) {
        MonthlySummary& summary = summaries[user_id];
        for (int day = 1; day <= total_days; day++) {
            if (user_data.find(day) == user_data.end()) {
                DailyCellData absent;
                absent.user_name = summary.user_name;
                absent.user_code = summary.user_code;
                absent.check_in = "--:--";
                absent.check_out = "--:--";
                absent.status = STATUS_ABSENT;
                user_data[day] = absent;
                summary.absent_days++;
            } else {
                DailyCellData& cell = user_data[day];
                if (cell.status == STATUS_NORMAL) summary.normal_days++;
                else if (cell.status == STATUS_LATE) {
                    summary.late_count++;
                    summary.total_late_minutes += cell.late_minutes;
                } else if (cell.status == STATUS_EARLY) {
                    summary.early_count++;
                    summary.total_early_minutes += cell.early_minutes;
                } else if (cell.status == STATUS_ABSENT) {
                    summary.absent_days++;
                }
            }
        }
    }

    // 5. 生成 Excel (调用已有的私有方法)
    lxw_workbook* workbook = workbook_new(output_path.c_str());
    if (!workbook) return false;

    // 写入各 Sheet
    writeSummarySheet(workbook, summaries);
    
    // 注意：writeShiftSheet 依赖年月，这里传入起始日期的年月作为近似
    int year = extractYearFromTimestamp(start_ts);
    int month = extractMonthFromTimestamp(start_ts);
    writeShiftSheet(workbook, target_users, year, month);

    // 写入原始记录 (需过滤)
    std::vector<AttendanceRecord> filtered_records;
    if (user_id_filter != -1) {
        for(const auto& r : records) { if(r.user_id == user_id_filter) filtered_records.push_back(r); }
        writeRecordSheet(workbook, filtered_records);
        writeExceptionSheet(workbook, filtered_records, summaries, detail_data, year, month);
    } else {
        writeRecordSheet(workbook, records);
        writeExceptionSheet(workbook, records, summaries, detail_data, year, month);
    }

    // 写入明细表 (复用逻辑，因为 keys 已经被处理成 1..total_days)
    writeDetailSheet(workbook, detail_data, total_days);

    workbook_close(workbook);
    std::cout << "[Success] 报表生成: " << output_path << std::endl;
    return true;
}

/**
 * @brief 生成精细化月度报表
 * @param month_str 月份字符串 "YYYY-MM"
 * @param output_path 输出文件路径
 * @return 是否成功
 */
bool ReportGenerator::exportDetailedReport(const std::string& month_str, 
                                         const std::string& output_path) {
    std::cout << "[ReportGenerator] 生成精细化月度报表: " << month_str << std::endl;
    
    // 解析月份
    std::tm tm = {};
    std::stringstream ss(month_str + "-01");
    ss >> std::get_time(&tm, "%Y-%m-%d");
    
    if (ss.fail()) {
        std::cerr << "[Error] 月份格式错误，请使用YYYY-MM格式" << std::endl;
        return false;
    }
    
    int year = tm.tm_year + 1900;
    int month = tm.tm_mon + 1;
    int days_in_month = getDaysInMonth(year, month);
    
    // 计算时间范围
    std::tm start_tm = tm;
    start_tm.tm_mday = 1;
    start_tm.tm_hour = 0;
    start_tm.tm_min = 0;
    start_tm.tm_sec = 0;
    long long start_ts = std::mktime(&start_tm);
    
    std::tm end_tm = start_tm;
    end_tm.tm_mon += 1;
    end_tm.tm_mday = 0;
    end_tm.tm_hour = 23;
    end_tm.tm_min = 59;
    end_tm.tm_sec = 59;
    long long end_ts = std::mktime(&end_tm);
    
    // 获取数据
    std::vector<AttendanceRecord> records = db_get_records(start_ts, end_ts);
    std::vector<UserData> users = db_get_all_users_info();
    
    // 处理数据
    std::map<int, std::map<int, DailyCellData>> detail_data;
    std::map<int, MonthlySummary> summaries;
    
    processAttendanceData(records, users, start_ts, end_ts, detail_data, summaries);
    
    // 创建Excel工作簿（要求五个Sheet）
    lxw_workbook* workbook = workbook_new(output_path.c_str());
    if (!workbook) {
        std::cerr << "[Error] 无法创建工作簿: " << output_path << std::endl;
        return false;
    }
     
    // Sheet 1: 汇总
    writeSummarySheet(workbook, summaries);
    
    // Sheet 2: 排班
    writeShiftSheet(workbook, users, year, month);
    
    // Sheet 3: 原始记录
    writeRecordSheet(workbook, records);
    
    // Sheet 4: 异常
     writeExceptionSheet(workbook, records, summaries, detail_data, year, month);
    
    // Sheet 5: 明细
    writeDetailSheet(workbook, detail_data, days_in_month);
    
    // 保存文件
    workbook_close(workbook);
    
    std::cout << "[Success] 精细化报表生成成功！" << std::endl;
    std::cout << "  文件: " << output_path << std::endl;
    std::cout << "  工作表数: 5" << std::endl;
    std::cout << "  月份: " << month_str << " (" << days_in_month << "天)" << std::endl;
    std::cout << "  员工数: " << users.size() << std::endl;
    std::cout << "  记录数: " << records.size() << std::endl;
    
    return true;
}

/**
 * @brief 导出报表到指定路径
 * @param type 报表类型
 * @param start_date 查询起始日期 "YYYY-MM-DD"
 * @param end_date 查询结束日期 "YYYY-MM-DD"
 * @param output_path 输出文件路径 (e.g. "output/usb_sim/report.xlsx")
 * @return true 导出成功, false 失败
 */
bool ReportGenerator::exportReport(ReportType type, 
                                   const std::string& start_date, 
                                   const std::string& end_date, 
                                   const std::string& output_path) {
    
    //获取时间范围
    long long start_ts = parseDateToTimestamp(start_date, false);
    long long end_ts = parseDateToTimestamp(end_date, true);

    // 从数据库获取原始记录
    // 注意：这里使用 data/db_storage.h 中的接口
    std::vector<AttendanceRecord> records = db_get_records(start_ts, end_ts);

    //显式按时间戳排序，确保"最早"是签到，"最晚"是签退
    std::sort(records.begin(), records.end(), [](const AttendanceRecord& a, const AttendanceRecord& b) {
        return a.timestamp < b.timestamp;
    });

    if (records.empty()) {
        std::cout << "[Report] No records found for range." << std::endl;
        // 即使没有数据，也可以生成一个空表头的文件
    }

    // 数据处理：将原始流水按 (User + Date) 聚合
    // Key: "UserID_YYYY-MM-DD", Value: DailySummary
    std::map<std::string, DailySummary> summary_map; //这里只保留这一行定义

    for (const auto& rec : records) {
        std::string date_str = formatDate(rec.timestamp);
        std::string key = std::to_string(rec.user_id) + "_" + date_str;

        if (summary_map.find(key) == summary_map.end()) {
            
            DailySummary ds; 
            
            ds.date = date_str;
            ds.name = rec.user_name;
            ds.dept = rec.dept_name;
            ds.check_in = formatTime(rec.timestamp); // 第一条作为签到
            ds.check_out = formatTime(rec.timestamp); // 暂时也作为签退
            
            //映射具体状态文本
            if (rec.status == 0) ds.status = "正常";
            else if (rec.status == 1) ds.status = "迟到";
            else if (rec.status == 2) ds.status = "早退";
            else ds.status = "异常"; // 其他状态
            
            ds.is_abnormal = (rec.status != 0);
            summary_map[key] = ds;
        } else {
            // 更新签退时间（因为已排序，后来的时间肯定更晚）
            summary_map[key].check_out = formatTime(rec.timestamp);
            
            // [优化] 如果出现异常，覆盖状态文本 (优先级: 迟到/早退 > 正常)
            if (rec.status != 0) {
                if (rec.status == 1) summary_map[key].status = "迟到";
                else if (rec.status == 2) summary_map[key].status = "早退";
                else summary_map[key].status = "异常";
                
                summary_map[key].is_abnormal = true;
            }
        }
    }

    // 获取所有员工信息 
    std::vector<UserData> all_users = db_get_all_users_info();

    // 遍历查询范围内的每一天
    for (long long t = start_ts; t <= end_ts; t += 86400) {
        std::string curr_date = formatDate(t);
        // 遍历每一个员工
        for (const auto& user : all_users) {
            std::string key = std::to_string(user.id) + "_" + curr_date;
            
            // 如果map里没有这个key，说明当天没打卡，补全“缺勤”
            if (summary_map.find(key) == summary_map.end()) {
                DailySummary ds;
                ds.date = curr_date;
                ds.name = user.name;
                ds.dept = user.dept_name;
                ds.check_in = "--:--";
                ds.check_out = "--:--";
                ds.status = "缺勤"; // 【关键】标记为缺勤
                ds.is_abnormal = true;
                summary_map[key] = ds;
            }
        }
    }

    //使用 libxlsxwriter 写入 Excel
    lxw_workbook  *workbook  = workbook_new(output_path.c_str());
    if (!workbook) {
        std::cerr << "[Error] Cannot create workbook: " << output_path << std::endl;
        return false;
    }
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, "考勤月报");

    // 设置样式
    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0xDDDDDD); // 浅灰背景
    format_set_border(header_format, LXW_BORDER_THIN);

    lxw_format *normal_format = workbook_add_format(workbook);
    format_set_border(normal_format, LXW_BORDER_THIN);

    lxw_format *red_format = workbook_add_format(workbook); // 异常标红
    format_set_font_color(red_format, LXW_COLOR_RED);
    format_set_border(red_format, LXW_BORDER_THIN);

    // 写入表头
    const char* headers[] = {"日期", "姓名", "部门", "签到时间", "签退时间", "状态"};
    for (int i = 0; i < 6; i++) {
        worksheet_write_string(worksheet, 0, i, headers[i], header_format);
    }
    // 设置列宽
    worksheet_set_column(worksheet, 0, 0, 15, NULL); // 日期
    worksheet_set_column(worksheet, 3, 4, 12, NULL); // 时间

    // 写入数据
    int row = 1;
    for (auto const& [key, val] : summary_map) {
        lxw_format *current_format = val.is_abnormal ? red_format : normal_format;

        worksheet_write_string(worksheet, row, 0, val.date.c_str(), current_format);
        worksheet_write_string(worksheet, row, 1, val.name.c_str(), current_format);
        worksheet_write_string(worksheet, row, 2, val.dept.c_str(), current_format);
        worksheet_write_string(worksheet, row, 3, val.check_in.c_str(), current_format);
        
        // 如果签到签退时间相同，说明只打了一次卡，签退显示为空
        if (val.check_in == val.check_out) {
             worksheet_write_string(worksheet, row, 4, "--:--", current_format);
        } else {
             worksheet_write_string(worksheet, row, 4, val.check_out.c_str(), current_format);
        }
        
        worksheet_write_string(worksheet, row, 5, val.status.c_str(), current_format);
        row++;
    }

    workbook_close(workbook);
    std::cout << "[Success] Report generated at: " << output_path << std::endl;
    return true;
}

/**
 * @brief 导出周报表
 * @param week_start_date 查询起始日期 "YYYY-MM-DD"（该周的第一天）
 * @param output_path 输出文件路径
 * @return true 导出成功, false 失败
 */
bool ReportGenerator::exportWeeklyReport(const std::string& week_start_date,  const std::string& output_path) {
    long long start_ts = parseDateToTimestamp(week_start_date, false);
    if (start_ts == 0){
        return false;
    }
    
    long long end_ts = start_ts + 6 * 86400; // 一周7天
    std::string end_date = formatDate(end_ts);

    return exportReport(ReportType::WEEKLY, week_start_date, end_date, output_path);
}

/**
 * @brief 导出部门报表
 * @param department_name 部门名称
 * @param start_date 查询起始日期 "YYYY-MM-DD"
 * @param end_date 查询结束日期 "YYYY-MM-DD"
 * @param output_path 输出文件路径
 * @return true 导出成功, false 失败
 */
bool ReportGenerator::exportDepartmentReport(const std::string& department_name, 
                                              const std::string& start_date, 
                                              const std::string& end_date, 
                                              const std::string& output_path) {
    // 目前与普通报表相同，未来可扩展为只包含某部门员工
    return exportReport(ReportType::DEPARTMENT, start_date, end_date, output_path);
}
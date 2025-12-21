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

ReportGenerator::ReportGenerator() {}
ReportGenerator::~ReportGenerator() {}

// 辅助：解析日期字符串 "2024-01-01" 到秒级时间戳
long long ReportGenerator::parseDateToTimestamp(const std::string& date_str, bool is_end_of_day) {
    std::tm tm = {};
    std::stringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    
    if (is_end_of_day) {
        tm.tm_hour = 23; tm.tm_min = 59; tm.tm_sec = 59;
    } else {
        tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
    }
    return std::mktime(&tm);
}

std::string ReportGenerator::formatTime(long long timestamp) {
    if (timestamp == 0) return "--:--";
    std::time_t t = (std::time_t)timestamp;
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));
    return std::string(buf);
}

std::string ReportGenerator::formatDate(long long timestamp) {
    std::time_t t = (std::time_t)timestamp;
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return std::string(buf);
}

bool ReportGenerator::exportReport(ReportType type, 
                                   const std::string& start_date, 
                                   const std::string& end_date, 
                                   const std::string& output_path) {
    
    // 1. 获取时间范围
    long long start_ts = parseDateToTimestamp(start_date, false);
    long long end_ts = parseDateToTimestamp(end_date, true);

    // 2. 从数据库获取原始记录
    // 注意：这里使用 data/db_storage.h 中的接口
    std::vector<AttendanceRecord> records = db_get_records(start_ts, end_ts);

    // [优化点 1] 显式按时间戳排序，确保"最早"是签到，"最晚"是签退
    std::sort(records.begin(), records.end(), [](const AttendanceRecord& a, const AttendanceRecord& b) {
        return a.timestamp < b.timestamp;
    });

    if (records.empty()) {
        std::cout << "[Report] No records found for range." << std::endl;
        // 即使没有数据，也可以生成一个空表头的文件
    }

    // 3. 数据处理：将原始流水按 (User + Date) 聚合
    // Key: "UserID_YYYY-MM-DD", Value: DailySummary
    std::map<std::string, DailySummary> summary_map; // 【注意】这里只保留这一行定义

    for (const auto& rec : records) {
        std::string date_str = formatDate(rec.timestamp);
        std::string key = std::to_string(rec.user_id) + "_" + date_str;

        if (summary_map.find(key) == summary_map.end()) {
            // 【关键修复】这里定义了 ds 变量
            DailySummary ds; 
            
            ds.date = date_str;
            ds.name = rec.user_name;
            ds.dept = rec.dept_name;
            ds.check_in = formatTime(rec.timestamp); // 第一条作为签到
            ds.check_out = formatTime(rec.timestamp); // 暂时也作为签退
            
            // [优化点 3] 映射具体状态文本
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

    // 4. 使用 libxlsxwriter 写入 Excel
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
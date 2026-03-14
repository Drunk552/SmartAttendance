/**
 * @file report_generator.h
 * @brief 报表导出模块 (Phase 04 - Epic 4.2)
 */

#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include <string>
#include <vector>
#include <map>
#include "data/db_storage.h"
#include <xlsxwriter.h>

// 报表文件输出类型定义
enum class ReportType {
    ATTENDANCE_ALL,         // 考勤报表（全员）.xls
    ATTENDANCE_INDIVIDUAL,  // 考勤报表（个人）.xls
    EMPLOYEE_SETTINGS       // 员工设置表.xls
};

// 考勤状态定义
enum AttendanceStatus {
    STATUS_NORMAL   = 0, // 正常
    STATUS_LATE     = 1, // 迟到
    STATUS_EARLY    = 2, // 早退
    STATUS_ABSENT   = 3, // 旷工（有排班但无打卡）
    STATUS_NO_SHIFT = 4  // 未排班（根据规则 Q3 第一阶段：无排班直接终止，不应判为旷工）
};

class ReportGenerator {
public:
// 内部辅助结构：每日考勤汇总
struct DailySummary {
    std::string date;
    std::string name;
    std::string dept;
    std::string check_in;   // 最早打卡
    std::string check_out;  // 最晚打卡
    std::string status;     // 状态文本
    bool is_abnormal;       // 是否异常（用于标红）
    };

//异常记录结构体
struct ExceptionRecord {
    int user_id;            // 工号
    std::string user_name;  // 姓名
    std::string dept_name;  // 部门
    std::string date;       // 异常日期
    std::string exception_type; // 异常类型
    std::string exception_detail; // 异常详情
};

// 每日单元格数据
struct DailyCellData {
    std::string user_name;
    std::string user_code;
    std::string check_in;
    std::string check_out;
    int status;
    int late_minutes;
    int early_minutes;

    int check_in_status = 3;  // 记录上班打卡状态 (默认 3: ABSENT)
    int check_out_status = 3; // 记录下班打卡状态 (默认 3: ABSENT)
    long long check_in_timestamp = 0; // 记录上班打卡时间戳，用于比对早晚
    long long check_out_timestamp = 0; // 记录下班打卡时间戳，用于比对早晚

    int final_status; // 最终综合状态
    bool is_abnormal;
};

// 月度汇总数据
 struct MonthlySummary {
    std::string user_name;
    std::string user_code;
    std::string dept;
    int normal_days = 0;
    int late_count = 0;
    int total_late_minutes = 0;
    int early_count = 0;
    int total_early_minutes = 0;
    int absent_days = 0;
    int no_shift_days = 0; // 未排班天数（规则 Q3：无排班 -> 未排班，独立于旷工统计）
    };

    ReportGenerator();
    ~ReportGenerator();

    // 1. 导出全员考勤报表（包含5个Sheet）
    bool exportAllAttendanceReport(const std::string& start_date, const std::string& end_date, const std::string& output_path);

    // 2. 导出个人考勤报表（包含5个Sheet）
    bool exportIndividualAttendanceReport(int user_id, const std::string& start_date, const std::string& end_date, const std::string& output_path);

    // 3. 导出员工及考勤设置表（包含2个Sheet）
    bool exportSettingsReport(const std::string& output_path);
                                         
private:

    // 辅助函数：时间处理
    long long parseDateToTimestamp(const std::string& date_str, bool is_end_of_day);
    std::string formatTime(long long timestamp);
    std::string formatDate(long long timestamp);
    std::string formatMonth(int year, int month);
    int getDaysInMonth(int year, int month);
    int extractDayFromTimestamp(long long timestamp);
    int extractMonthFromTimestamp(long long timestamp);
    int extractYearFromTimestamp(long long timestamp);

    // 新增辅助函数
    std::map<int, ShiftInfo> db_get_user_monthly_shifts(int user_id, int year, int month);
    std::string formatDateString(int year, int month, int day);

    // 计算迟到/早退分钟数
    int calculateLateMinutes(long long timestamp, const ShiftInfo& shift);
    int calculateEarlyMinutes(long long timestamp, const ShiftInfo& shift);

    // 数据库访问函数
    std::vector<AttendanceRecord> db_get_records(long long start_ts, long long end_ts);
    std::vector<UserData> db_get_all_users_info();
    std::vector<UserData> db_get_users_by_dept(const std::string& dept_name);

    // 样式创建函数
    lxw_format* createHeaderFormat(lxw_workbook* workbook);
    lxw_format* createNormalFormat(lxw_workbook* workbook);
    lxw_format* createRedFormat(lxw_workbook* workbook);
    lxw_format* createGreenFormat(lxw_workbook* workbook);
    lxw_format* createYellowFormat(lxw_workbook* workbook);

    // 数据辅助函数
    std::string getAttendanceSymbol(const std::string& check_in, const std::string& check_out, int status);
    lxw_color_t getStatusColor(int status);

    // 核心数据处理函数
    void processAttendanceData(
        const std::vector<AttendanceRecord>& records,
        const std::vector<UserData>& users,
        long long start_ts, long long end_ts,
        std::map<int, std::map<int, DailyCellData>>& detail_data,
        std::map<int, MonthlySummary>& summaries);

    // ==== 考勤报表相关的 5 个 Sheet 写入函数 ====
    // 1. 写入【排班信息表】
    void writeShiftInfoSheet(lxw_workbook* workbook, 
                         const std::vector<UserData>& users, 
                         const std::vector<DeptInfo>& depts,
                         const std::vector<ShiftInfo>& shifts,
                         const std::string& start_date,
                         const std::string& end_date,
                         int year, int month, int days_in_month);
    
    // 2. 写入【考勤汇总表】
    void writeSummarySheet(lxw_workbook* workbook, 
                        const std::map<int, MonthlySummary>& summaries, 
                        const std::string& start_date, 
                        const std::string& end_date);
    
    // 3. 写入【考勤记录表】
    void writeRecordSheet(lxw_workbook* workbook, 
                        const std::vector<AttendanceRecord>& records);
    
    // 4. 写入【考勤异常统计表】
    void writeAbnormalSheet(lxw_workbook* workbook, 
                        const std::vector<AttendanceRecord>& abnormal_records, 
                        const std::string& start_date, 
                        const std::string& end_date);

    // 5. 写入【考勤明细表】
    void writeDetailSheet(lxw_workbook* workbook, 
                        const std::map<int, std::map<int, DailyCellData>>& detail_data, 
                        const std::map<int, MonthlySummary>& summaries,
                        const std::string& start_date, 
                        const std::string& end_date,
                        int year, int month, int days_in_month);

    // ==== 设置表相关的 2 个 Sheet 写入函数 ====

    // 6. 写入【员工设置表】
    void writeEmployeeSettingsSheet(lxw_workbook* workbook, 
                        const std::vector<UserData>& users,
                        const std::string& start_date,
                        int year, int month, int days_in_month);
    
    // 7. 写入【考勤设置表】
    void writeAttendanceSettingsSheet(lxw_workbook* workbook, 
                        const RuleConfig& config, 
                        const std::vector<DeptInfo>& depts, 
                        const std::vector<ShiftInfo>& shifts);
};

#endif // REPORT_GENERATOR_H
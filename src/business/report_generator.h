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

// 报表类型定义
enum class ReportType {
    SUMMARY,        // 考勤汇总表 (目前主要实现这个)
    ABNORMAL,       // 异常记录表
    EMPLOYEE_INFO,  // 员工信息表
    WEEKLY,         // 周报表
    DEPARTMENT      // 部门报表
};

// 考勤状态定义
enum AttendanceStatus {
    STATUS_NORMAL = 0,
    STATUS_LATE = 1,
    STATUS_EARLY = 2,
    STATUS_ABSENT = 3
};

class ReportGenerator {
public:
    ReportGenerator();
    ~ReportGenerator();

    /**
     * @brief 导出报表到指定路径
     * @param type 报表类型
     * @param start_date 查询起始日期 "YYYY-MM-DD"
     * @param end_date 查询结束日期 "YYYY-MM-DD"
     * @param output_path 输出文件路径 (e.g. "output/usb_sim/report.xlsx")
     * @return true 导出成功, false 失败
     */
    bool exportReport(ReportType type, 
                      const std::string& start_date, 
                      const std::string& end_date, 
                      const std::string& output_path);

    /**
     * @brief 生成精细化月度报表
     * @param month_str 月份字符串 "YYYY-MM"
     * @param output_path 输出文件路径
     * @return 是否成功
     */
    bool exportDetailedReport(const std::string& month_str, 
                              const std::string& output_path);

    /**
     * @brief 导出周报表
     * @param week_start_date 查询起始日期 "YYYY-MM-DD"（该周的第一天）
     * @param output_path 输出文件路径
     * @return true 导出成功, false 失败
     */
    bool exportWeeklyReport(const std::string& week_start_date, 
                            const std::string& output_path);

    /**
     * @brief 导出部门报表
     * @param department_name 部门名称
     * @param start_date 查询起始日期 "YYYY-MM-DD"
     * @param end_date 查询结束日期 "YYYY-MM-DD"
     * @param output_path 输出文件路径
     * @return true 导出成功, false 失败
     */
    bool exportDepartmentReport(const std::string& department_name, 
                                const std::string& start_date, 
                                const std::string& end_date, 
                                const std::string& output_path);

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

    // 每日单元格数据
    struct DailyCellData {
        std::string user_name;
        std::string user_code;
        std::string check_in;
        std::string check_out;
        int status;
        int late_minutes;
        int early_minutes;
    };

    // 月度汇总数据
    struct MonthlySummary {
        std::string user_name;
        std::string user_code;
        std::string dept;
        int normal_days;
        int late_count;
        int total_late_minutes;
        int early_count;
        int total_early_minutes;
        int absent_days;
    };

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

    // Excel工作表写入函数
    void writeSummarySheet(lxw_workbook* workbook, 
                           const std::map<int, MonthlySummary>& summaries);
    void writeDetailSheet(lxw_workbook* workbook,
                          const std::map<int, std::map<int, DailyCellData>>& detail_data,
                          int days_in_month);
};

#endif // REPORT_GENERATOR_H
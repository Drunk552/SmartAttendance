/**
 * @file report_generator.h
 * @brief 报表导出模块 (Phase 04 - Epic 4.2)
 */

#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include <string>
#include <vector>
#include "data/db_storage.h"

// 报表类型定义
enum class ReportType {
    SUMMARY,        // 考勤汇总表 (目前主要实现这个)
    ABNORMAL,       // 异常记录表
    EMPLOYEE_INFO   // 员工信息表
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

private:
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

    // 辅助函数：将时间戳转换为 YYYY-MM-DD 格式
    long long parseDateToTimestamp(const std::string& date_str, bool is_end_of_day);
    std::string formatTime(long long timestamp);
    std::string formatDate(long long timestamp);
};

#endif // REPORT_GENERATOR_H
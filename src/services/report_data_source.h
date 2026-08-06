#ifndef SMART_ATTENDANCE_SERVICES_REPORT_DATA_SOURCE_H
#define SMART_ATTENDANCE_SERVICES_REPORT_DATA_SOURCE_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace smart_attendance::services {

struct ReportAttendanceRecord {
    int id{0};
    int user_id{0};
    std::string user_name;
    std::string dept_name;
    long long timestamp{0};
    int status{0};
    std::string image_path;
    int minutes_late{0};
    int minutes_early{0};
};

struct ReportUser {
    int id{0};
    std::string name;
    std::string password;
    std::string card_id;
    int role{0};
    int dept_id{0};
    int default_shift_id{0};
    std::map<int, int> monthly_schedule;
    std::string dept_name;
    std::vector<std::uint8_t> face_feature;
    std::string avatar_path;
    std::vector<std::uint8_t> fingerprint_feature;
    std::string position;
};

struct ReportDepartment {
    int id{0};
    std::string name;
    int company_id{0};
    std::string company_name;
};

struct ReportShift {
    int id{0};
    std::string name;
    std::string s1_start;
    std::string s1_end;
    std::string s2_start;
    std::string s2_end;
    std::string s3_start;
    std::string s3_end;
    int cross_day{0};
};

struct ReportRules {
    std::string company_name;
    int late_threshold{0};
    int early_leave_threshold{0};
    int device_id{0};
    int volume{0};
    int screensaver_time{0};
    int max_admins{0};
    int relay_delay{0};
    int wiegand_fmt{0};
    int duplicate_punch_limit{0};
    std::string language;
    std::string date_format;
    int return_home_delay{0};
    int warning_record_count{0};
    int sat_work{0};
    int sun_work{0};
};

/** @brief 报表生成所需的只读数据边界，不暴露 SQLite 或旧数据库 DTO。 */
class IReportDataSource {
public:
    virtual ~IReportDataSource() = default;

    virtual std::vector<ReportAttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) = 0;
    virtual std::vector<ReportAttendanceRecord> recordsForUser(
        int userId, long long startTimestamp, long long endTimestamp) = 0;
    virtual std::vector<ReportUser> users() = 0;
    virtual std::optional<ReportUser> user(int userId) = 0;
    virtual std::vector<ReportDepartment> departments() = 0;
    virtual std::vector<ReportShift> shifts() = 0;
    virtual ReportRules globalRules() = 0;
    virtual std::optional<ReportShift> shiftForUserAt(
        int userId, long long timestamp) = 0;
};

} // namespace smart_attendance::services

#endif // SMART_ATTENDANCE_SERVICES_REPORT_DATA_SOURCE_H

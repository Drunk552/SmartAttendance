#ifndef SMART_ATTENDANCE_CORE_MODEL_LEGACY_DATABASE_MODELS_H
#define SMART_ATTENDANCE_CORE_MODEL_LEGACY_DATABASE_MODELS_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Transitional DTOs shared by the legacy SQLite adapters. The legacy field
// names are retained for schema compatibility, while ownership belongs to the
// core model layer.
namespace smart_attendance::core {

struct LegacyDeptInfo {
    int id;
    std::string name;
    int company_id;
    std::string company_name;
    LegacyDeptInfo() : id(0), company_id(0) {}
};

struct LegacyShiftInfo {
    int id;
    std::string name;
    std::string s1_start;
    std::string s1_end;
    std::string s2_start;
    std::string s2_end;
    std::string s3_start;
    std::string s3_end;
    int cross_day;
};

struct LegacyDeptScheduleEntry {
    int dept_id;
    int day_of_week;
    int shift_id;
};

struct LegacyDeptScheduleView {
    int dept_id;
    std::string dept_name;
    int shifts[7];
};

struct LegacyRuleConfig {
    std::string company_name;
    int late_threshold;
    int early_leave_threshold;
    int device_id;
    int volume;
    int screensaver_time;
    int max_admins;
    int relay_delay;
    int wiegand_fmt;
    int duplicate_punch_limit;
    std::string language;
    std::string date_format;
    int return_home_delay;
    int warning_record_count;
    int sat_work;
    int sun_work;
};

struct LegacyBellSchedule {
    int id;
    std::string time;
    int duration;
    int days_mask;
    bool enabled;
};

struct LegacyUserData {
    int id;
    std::string name;
    std::string password;
    std::string card_id;
    int role;
    int dept_id;
    int default_shift_id;
    std::map<int, int> monthly_schedule;
    std::string dept_name;
    std::vector<std::uint8_t> face_feature;
    std::string avatar_path;
    std::vector<std::uint8_t> fingerprint_feature;
    std::string position;
    LegacyUserData() : id(0), role(0), dept_id(0), default_shift_id(0) {}
};

enum class LegacyDbUserLookupStatus { Found, NotFound, ReadError };
struct LegacyDbUserLookupResult {
    LegacyDbUserLookupStatus status;
    std::optional<LegacyUserData> user;
};

enum class LegacyDbUserPageStatus { Success, InvalidArgument, ReadError };
constexpr std::size_t kMaxLegacyDbUserBasicPageSize = 64U;
struct LegacyDbUserPageResult {
    LegacyDbUserPageStatus status;
    std::vector<LegacyUserData> users;
    bool has_more;
};

enum class LegacyDbTextLookupStatus { Found, NotFound, ReadError };
struct LegacyDbTextLookupResult {
    LegacyDbTextLookupStatus status;
    std::optional<std::string> value;
};

struct LegacyAttendanceRecord {
    int id;
    int user_id;
    std::string user_name;
    std::string dept_name;
    long long timestamp;
    int status;
    std::string image_path;
    int minutes_late;
    int minutes_early;
};

enum class LegacyDbAttendanceQueryStatus { Success, InvalidArgument, ReadError };
constexpr std::size_t kMaxLegacyDbAttendanceQuerySize = 512U;
struct LegacyDbAttendanceQueryResult {
    LegacyDbAttendanceQueryStatus status;
    std::vector<LegacyAttendanceRecord> records;
    bool has_more;
};

struct LegacySystemStats {
    int total_employees;
    int total_admins;
    int total_faces;
    int total_fingerprints;
    int total_cards;
};

struct LegacyCompanyInfo {
    int id;
    std::string name;
    std::string code;
    std::string address;
    std::string contact_phone;
    std::string contact_email;
    std::string created_at;
    std::string updated_at;
    LegacyCompanyInfo() : id(0) {}
};

} // namespace smart_attendance::core

#endif

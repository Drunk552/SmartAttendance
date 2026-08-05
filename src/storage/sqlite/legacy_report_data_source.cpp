#include "legacy_report_data_source.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

std::vector<AttendanceRecord> LegacyReportDataSource::records(
    long long startTimestamp, long long endTimestamp) {
    return db_get_records(startTimestamp, endTimestamp);
}

std::vector<AttendanceRecord> LegacyReportDataSource::recordsForUser(
    int userId, long long startTimestamp, long long endTimestamp) {
    return db_get_records_by_user(userId, startTimestamp, endTimestamp);
}

std::vector<UserData> LegacyReportDataSource::users() {
    return db_get_all_users();
}

std::optional<UserData> LegacyReportDataSource::user(int userId) {
    return db_get_user_info(userId);
}

std::vector<DeptInfo> LegacyReportDataSource::departments() {
    return db_get_departments();
}

std::vector<ShiftInfo> LegacyReportDataSource::shifts() {
    return db_get_shifts();
}

RuleConfig LegacyReportDataSource::globalRules() {
    return db_get_global_rules();
}

std::optional<ShiftInfo> LegacyReportDataSource::shiftForUserAt(
    int userId, long long timestamp) {
    return db_get_user_shift_smart(userId, timestamp);
}

} // namespace smart_attendance::storage::sqlite

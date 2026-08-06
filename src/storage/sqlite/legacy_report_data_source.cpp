#include "legacy_report_data_source.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {
namespace {

services::ReportAttendanceRecord mapRecord(const AttendanceRecord& source) {
    return {source.id, source.user_id, source.user_name, source.dept_name,
            source.timestamp, source.status, source.image_path,
            source.minutes_late, source.minutes_early};
}

services::ReportUser mapUser(const UserData& source) {
    services::ReportUser user;
    user.id = source.id;
    user.name = source.name;
    user.password = source.password;
    user.card_id = source.card_id;
    user.role = source.role;
    user.dept_id = source.dept_id;
    user.default_shift_id = source.default_shift_id;
    user.monthly_schedule = source.monthly_schedule;
    user.dept_name = source.dept_name;
    user.face_feature.assign(source.face_feature.begin(), source.face_feature.end());
    user.avatar_path = source.avatar_path;
    user.fingerprint_feature = source.fingerprint_feature;
    user.position = source.position;
    return user;
}

services::ReportDepartment mapDepartment(const DeptInfo& source) {
    return {source.id, source.name, source.company_id, source.company_name};
}

services::ReportShift mapShift(const ShiftInfo& source) {
    return {source.id, source.name, source.s1_start, source.s1_end,
            source.s2_start, source.s2_end, source.s3_start, source.s3_end,
            source.cross_day};
}

services::ReportRules mapRules(const RuleConfig& source) {
    services::ReportRules rules;
    rules.company_name = source.company_name;
    rules.late_threshold = source.late_threshold;
    rules.early_leave_threshold = source.early_leave_threshold;
    rules.device_id = source.device_id;
    rules.volume = source.volume;
    rules.screensaver_time = source.screensaver_time;
    rules.max_admins = source.max_admins;
    rules.relay_delay = source.relay_delay;
    rules.wiegand_fmt = source.wiegand_fmt;
    rules.duplicate_punch_limit = source.duplicate_punch_limit;
    rules.language = source.language;
    rules.date_format = source.date_format;
    rules.return_home_delay = source.return_home_delay;
    rules.warning_record_count = source.warning_record_count;
    rules.sat_work = source.sat_work;
    rules.sun_work = source.sun_work;
    return rules;
}

template <typename Source, typename Target, typename Mapper>
std::vector<Target> mapVector(const std::vector<Source>& source, Mapper mapper) {
    std::vector<Target> result;
    result.reserve(source.size());
    for (const auto& item : source) {
        result.push_back(mapper(item));
    }
    return result;
}

} // namespace

std::vector<services::ReportAttendanceRecord> LegacyReportDataSource::records(
    long long startTimestamp, long long endTimestamp) {
    return mapVector<AttendanceRecord, services::ReportAttendanceRecord>(
        db_get_records(startTimestamp, endTimestamp), mapRecord);
}

std::vector<services::ReportAttendanceRecord>
LegacyReportDataSource::recordsForUser(
    int userId, long long startTimestamp, long long endTimestamp) {
    return mapVector<AttendanceRecord, services::ReportAttendanceRecord>(
        db_get_records_by_user(userId, startTimestamp, endTimestamp), mapRecord);
}

std::vector<services::ReportUser> LegacyReportDataSource::users() {
    return mapVector<UserData, services::ReportUser>(db_get_all_users(), mapUser);
}

std::optional<services::ReportUser> LegacyReportDataSource::user(int userId) {
    const auto source = db_get_user_info(userId);
    return source ? std::optional<services::ReportUser>(mapUser(*source))
                  : std::nullopt;
}

std::vector<services::ReportDepartment> LegacyReportDataSource::departments() {
    return mapVector<DeptInfo, services::ReportDepartment>(
        db_get_departments(), mapDepartment);
}

std::vector<services::ReportShift> LegacyReportDataSource::shifts() {
    return mapVector<ShiftInfo, services::ReportShift>(db_get_shifts(), mapShift);
}

services::ReportRules LegacyReportDataSource::globalRules() {
    return mapRules(db_get_global_rules());
}

std::optional<services::ReportShift> LegacyReportDataSource::shiftForUserAt(
    int userId, long long timestamp) {
    const auto source = db_get_user_shift_smart(userId, timestamp);
    return source ? std::optional<services::ReportShift>(mapShift(*source))
                  : std::nullopt;
}

} // namespace smart_attendance::storage::sqlite

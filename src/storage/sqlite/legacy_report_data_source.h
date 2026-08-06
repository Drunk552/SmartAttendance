#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_REPORT_DATA_SOURCE_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_REPORT_DATA_SOURCE_H

#include "services/report_data_source.h"

namespace smart_attendance::storage::sqlite {

class LegacyReportDataSource final : public services::IReportDataSource {
public:
    std::vector<services::ReportAttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) override;
    std::vector<services::ReportAttendanceRecord> recordsForUser(
        int userId, long long startTimestamp, long long endTimestamp) override;
    std::vector<services::ReportUser> users() override;
    std::optional<services::ReportUser> user(int userId) override;
    std::vector<services::ReportDepartment> departments() override;
    std::vector<services::ReportShift> shifts() override;
    services::ReportRules globalRules() override;
    std::optional<services::ReportShift> shiftForUserAt(
        int userId, long long timestamp) override;
};

} // namespace smart_attendance::storage::sqlite

#endif

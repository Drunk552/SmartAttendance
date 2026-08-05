#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_REPORT_DATA_SOURCE_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_REPORT_DATA_SOURCE_H

#include "services/report_data_source.h"

namespace smart_attendance::storage::sqlite {

class LegacyReportDataSource final : public services::IReportDataSource {
public:
    std::vector<AttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) override;
    std::vector<AttendanceRecord> recordsForUser(
        int userId, long long startTimestamp, long long endTimestamp) override;
    std::vector<UserData> users() override;
    std::optional<UserData> user(int userId) override;
    std::vector<DeptInfo> departments() override;
    std::vector<ShiftInfo> shifts() override;
    RuleConfig globalRules() override;
    std::optional<ShiftInfo> shiftForUserAt(
        int userId, long long timestamp) override;
};

} // namespace smart_attendance::storage::sqlite

#endif

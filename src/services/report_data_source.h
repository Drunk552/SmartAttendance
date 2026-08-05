#ifndef SMART_ATTENDANCE_SERVICES_REPORT_DATA_SOURCE_H
#define SMART_ATTENDANCE_SERVICES_REPORT_DATA_SOURCE_H

#include "data/db_storage.h"

#include <optional>
#include <vector>

namespace smart_attendance::services {

/** @brief 报表生成所需的只读数据边界，不暴露 SQLite API。 */
class IReportDataSource {
public:
    virtual ~IReportDataSource() = default;

    virtual std::vector<AttendanceRecord> records(
        long long startTimestamp, long long endTimestamp) = 0;
    virtual std::vector<AttendanceRecord> recordsForUser(
        int userId, long long startTimestamp, long long endTimestamp) = 0;
    virtual std::vector<UserData> users() = 0;
    virtual std::optional<UserData> user(int userId) = 0;
    virtual std::vector<DeptInfo> departments() = 0;
    virtual std::vector<ShiftInfo> shifts() = 0;
    virtual RuleConfig globalRules() = 0;
    virtual std::optional<ShiftInfo> shiftForUserAt(
        int userId, long long timestamp) = 0;
};

} // namespace smart_attendance::services

#endif

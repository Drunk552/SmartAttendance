/**
 * @file punch_repositories.h
 * @brief 声明统一打卡链路所需的最小存储抽象。
 */

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_PUNCH_REPOSITORIES_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_PUNCH_REPOSITORIES_H

#include "core/attendance/punch_rule.h"
#include "core/common/result.h"
#include "storage/repository/repository_error.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace smart_attendance::storage {

struct ScheduledShift {
    int id;
    core::ShiftPeriod firstPeriod;
    core::ShiftPeriod secondPeriod;
};

struct AttendanceRules {
    int lateThresholdMinutes;
    int duplicatePunchLimitMinutes;
};

struct AttendanceEntry {
    int userId;
    int shiftId;
    std::int64_t timestamp;
    core::PunchStatus status;
    int minutesDifference;
    /** @brief 可选的 JPEG 抓拍数据；Repository 必须在 save 返回前完成消费。 */
    std::vector<std::uint8_t> snapshotJpeg{};
};

class IScheduleRepository {
public:
    virtual ~IScheduleRepository() = default;
    virtual Result<std::optional<ScheduledShift>, RepositoryError>
    findForUserAt(int userId, std::int64_t timestamp) = 0;
};

class IAttendanceRuleRepository {
public:
    virtual ~IAttendanceRuleRepository() = default;
    virtual Result<AttendanceRules, RepositoryError> load() = 0;
};

class IAttendanceRepository {
public:
    virtual ~IAttendanceRepository() = default;
    virtual Result<bool, RepositoryError> hasRecordInRange(
        int userId, std::int64_t startTimestamp, std::int64_t endTimestamp) = 0;
    virtual Result<void, RepositoryError> save(const AttendanceEntry& entry) = 0;
};

} // namespace smart_attendance::storage

#endif // SMART_ATTENDANCE_STORAGE_REPOSITORY_PUNCH_REPOSITORIES_H

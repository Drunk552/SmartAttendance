/**
 * @file schedule_repository.h
 * @brief 声明打卡流程使用的排班查询抽象。
 */

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_SCHEDULE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_SCHEDULE_REPOSITORY_H

#include "core/attendance/punch_rule.h"
#include "core/common/result.h"
#include "storage/repository/repository_error.h"

#include <cstdint>
#include <optional>

namespace smart_attendance::storage {

struct ScheduledShift {
    int id;
    core::ShiftPeriod firstPeriod;
    core::ShiftPeriod secondPeriod;
};

class IScheduleRepository {
public:
    virtual ~IScheduleRepository() = default;
    virtual Result<std::optional<ScheduledShift>, RepositoryError>
    findForUserAt(int userId, std::int64_t timestamp) = 0;
};

} // namespace smart_attendance::storage

#endif // SMART_ATTENDANCE_STORAGE_REPOSITORY_SCHEDULE_REPOSITORY_H

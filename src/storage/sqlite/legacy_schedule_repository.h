/**
 * @file legacy_schedule_repository.h
 * @brief 声明现有 SQLite 排班查询的 Repository 适配器。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SCHEDULE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SCHEDULE_REPOSITORY_H

#include "storage/repository/schedule_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyScheduleRepository final : public IScheduleRepository {
public:
    Result<std::optional<ScheduledShift>, RepositoryError>
    findForUserAt(int userId, std::int64_t timestamp) override;
};

} // namespace smart_attendance::storage::sqlite

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SCHEDULE_REPOSITORY_H

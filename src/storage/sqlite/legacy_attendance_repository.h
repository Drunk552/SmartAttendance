/**
 * @file legacy_attendance_repository.h
 * @brief 声明现有 SQLite 考勤规则与记录 Repository 适配器。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_ATTENDANCE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_ATTENDANCE_REPOSITORY_H

#include "storage/repository/attendance_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyAttendanceRuleRepository final : public IAttendanceRuleRepository {
public:
    Result<AttendanceRules, RepositoryError> load() override;
};

class LegacyAttendanceRepository final : public IAttendanceRepository {
public:
    Result<bool, RepositoryError> hasRecordInRange(
        int userId, std::int64_t startTimestamp, std::int64_t endTimestamp) override;
    Result<void, RepositoryError> save(const AttendanceEntry& entry) override;
};

} // namespace smart_attendance::storage::sqlite

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_ATTENDANCE_REPOSITORY_H

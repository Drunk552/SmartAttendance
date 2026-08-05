/**
 * @file legacy_punch_repositories.h
 * @brief 声明将现有 db_storage 接入统一打卡服务的过渡适配器。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_PUNCH_REPOSITORIES_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_PUNCH_REPOSITORIES_H

#include "storage/repository/punch_repositories.h"

namespace smart_attendance::storage::sqlite {

/**
 * @brief 通过进程级 db_storage 查询排班的过渡适配器。
 * @note 调用可能阻塞数据库 IO；Application 必须保证调用期间数据库保持打开。
 */
class LegacyScheduleRepository final : public IScheduleRepository {
public:
    Result<std::optional<ScheduledShift>, RepositoryError>
    findForUserAt(int userId, std::int64_t timestamp) override;
};

/** @brief 通过进程级 db_storage 读取打卡规则的过渡适配器。 */
class LegacyAttendanceRuleRepository final : public IAttendanceRuleRepository {
public:
    Result<AttendanceRules, RepositoryError> load() override;
};

/**
 * @brief 通过进程级 db_storage 查询和写入打卡记录的过渡适配器。
 * @note save 会解码 PunchService 提供的可选 JPEG 抓拍并沿用旧图片保存格式。
 */
class LegacyAttendanceRepository final : public IAttendanceRepository {
public:
    Result<bool, RepositoryError> hasRecordInRange(
        int userId, std::int64_t startTimestamp, std::int64_t endTimestamp) override;
    Result<void, RepositoryError> save(const AttendanceEntry& entry) override;
};

} // namespace smart_attendance::storage::sqlite

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_PUNCH_REPOSITORIES_H

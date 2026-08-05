/**
 * @file legacy_schedule_repository.cpp
 * @brief 实现旧排班 DAO 到 Repository 的映射。
 */

#include "legacy_schedule_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

Result<std::optional<ScheduledShift>, RepositoryError>
LegacyScheduleRepository::findForUserAt(int userId, std::int64_t timestamp) {
    using ResultType = Result<std::optional<ScheduledShift>, RepositoryError>;
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    try {
        const auto shift = db_get_user_shift_smart(userId, timestamp);
        if (!shift || shift->id <= 0) {
            return ResultType::success(std::nullopt);
        }
        return ResultType::success(ScheduledShift{
            shift->id,
            {shift->s1_start, shift->s1_end},
            {shift->s2_start, shift->s2_end}});
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

} // namespace smart_attendance::storage::sqlite

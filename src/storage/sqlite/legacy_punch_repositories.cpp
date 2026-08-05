/**
 * @file legacy_punch_repositories.cpp
 * @brief 实现 db_storage 到打卡 Repository 抽象的过渡映射。
 */

#include "legacy_punch_repositories.h"

#include "data/db_storage.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

namespace smart_attendance::storage::sqlite {
namespace {

int toLegacyStatus(core::PunchStatus status) noexcept {
    switch (status) {
        case core::PunchStatus::Normal: return 0;
        case core::PunchStatus::Late: return 1;
        case core::PunchStatus::Early: return 2;
        case core::PunchStatus::Absent: return 3;
    }
    return 3;
}

} // namespace

Result<std::optional<ScheduledShift>, RepositoryError>
LegacyScheduleRepository::findForUserAt(int userId, std::int64_t timestamp) {
    if (!data_is_open()) {
        return Result<std::optional<ScheduledShift>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    }

    try {
        const auto shift = db_get_user_shift_smart(userId, timestamp);
        if (!shift || shift->id <= 0) {
            return Result<std::optional<ScheduledShift>, RepositoryError>::success(
                std::nullopt);
        }
        return Result<std::optional<ScheduledShift>, RepositoryError>::success(
            ScheduledShift{
                shift->id,
                {shift->s1_start, shift->s1_end},
                {shift->s2_start, shift->s2_end}});
    } catch (...) {
        return Result<std::optional<ScheduledShift>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    }
}

Result<AttendanceRules, RepositoryError>
LegacyAttendanceRuleRepository::load() {
    if (!data_is_open()) {
        return Result<AttendanceRules, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    }

    try {
        const RuleConfig rules = db_get_global_rules();
        return Result<AttendanceRules, RepositoryError>::success(
            {rules.late_threshold, rules.duplicate_punch_limit});
    } catch (...) {
        return Result<AttendanceRules, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    }
}

Result<bool, RepositoryError> LegacyAttendanceRepository::hasRecordInRange(
    int userId,
    std::int64_t startTimestamp,
    std::int64_t endTimestamp) {
    if (!data_is_open()) {
        return Result<bool, RepositoryError>::failure(RepositoryError::ReadFailed);
    }

    try {
        return Result<bool, RepositoryError>::success(
            !db_get_records_by_user(userId, startTimestamp, endTimestamp).empty());
    } catch (...) {
        return Result<bool, RepositoryError>::failure(RepositoryError::ReadFailed);
    }
}

Result<void, RepositoryError>
LegacyAttendanceRepository::save(const AttendanceEntry& entry) {
    if (!data_is_open()) {
        return Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
    }

    try {
        cv::Mat snapshot;
        if (!entry.snapshotJpeg.empty()) {
            snapshot = cv::imdecode(entry.snapshotJpeg, cv::IMREAD_COLOR);
            if (snapshot.empty()) {
                return Result<void, RepositoryError>::failure(
                    RepositoryError::WriteFailed);
            }
        }
        if (!db_log_attendance_at(entry.userId,
                                  entry.shiftId,
                                  snapshot,
                                  toLegacyStatus(entry.status),
                                  entry.timestamp)) {
            return Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
        }
        return Result<void, RepositoryError>::success();
    } catch (...) {
        return Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
    }
}

} // namespace smart_attendance::storage::sqlite

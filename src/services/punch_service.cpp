/**
 * @file punch_service.cpp
 * @brief 实现统一打卡业务编排。
 */

#include "punch_service.h"

#include "core/attendance/punch_rule.h"

#include <limits>
#include <utility>

namespace smart_attendance::services {

PunchService::PunchService(
    storage::IScheduleRepository& scheduleRepository,
    storage::IAttendanceRuleRepository& ruleRepository,
    storage::IAttendanceRepository& attendanceRepository) noexcept
    : scheduleRepository_(scheduleRepository),
      ruleRepository_(ruleRepository),
      attendanceRepository_(attendanceRepository) {}

Result<PunchReceipt, PunchError> PunchService::punch(PunchRequest request) {
    if (request.userId <= 0 || request.timestamp < 0 ||
        request.localMinute < 0 || request.localMinute >= 1440) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::InvalidRequest);
    }

    const auto shiftResult = [&] {
        try {
            return scheduleRepository_.findForUserAt(
                request.userId, request.timestamp);
        } catch (...) {
            return Result<std::optional<storage::ScheduledShift>,
                          storage::RepositoryError>::failure(
                storage::RepositoryError::ReadFailed);
        }
    }();
    if (!shiftResult) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::ScheduleReadFailed);
    }
    if (!shiftResult.value()) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::NoShift);
    }
    const storage::ScheduledShift& shift = *shiftResult.value();

    const auto rulesResult = [&] {
        try {
            return ruleRepository_.load();
        } catch (...) {
            return Result<storage::AttendanceRules,
                          storage::RepositoryError>::failure(
                storage::RepositoryError::ReadFailed);
        }
    }();
    if (!rulesResult) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::RulesReadFailed);
    }
    const storage::AttendanceRules& rules = rulesResult.value();
    if (rules.lateThresholdMinutes < 0 || rules.duplicatePunchLimitMinutes < 0) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::InvalidRules);
    }

    if (rules.duplicatePunchLimitMinutes > 0) {
        constexpr std::int64_t kSecondsPerMinute = 60;
        const std::int64_t limit = rules.duplicatePunchLimitMinutes >
                                           std::numeric_limits<std::int64_t>::max() /
                                               kSecondsPerMinute
                                       ? std::numeric_limits<std::int64_t>::max()
                                       : static_cast<std::int64_t>(
                                             rules.duplicatePunchLimitMinutes) *
                                             kSecondsPerMinute;
        const std::int64_t start = request.timestamp > limit
                                       ? request.timestamp - limit
                                       : 0;
        const auto duplicateResult = [&] {
            try {
                return attendanceRepository_.hasRecordInRange(
                    request.userId, start, request.timestamp);
            } catch (...) {
                return Result<bool, storage::RepositoryError>::failure(
                    storage::RepositoryError::ReadFailed);
            }
        }();
        if (!duplicateResult) {
            return Result<PunchReceipt, PunchError>::failure(
                PunchError::AttendanceReadFailed);
        }
        if (duplicateResult.value()) {
            return Result<PunchReceipt, PunchError>::failure(
                PunchError::DuplicatePunch);
        }
    }

    const auto calculation = core::calculatePunch(
        request.localMinute,
        shift.firstPeriod,
        shift.secondPeriod,
        rules.lateThresholdMinutes);
    if (!calculation) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::InvalidShift);
    }

    const storage::AttendanceEntry entry{
        request.userId,
        shift.id,
        request.timestamp,
        calculation->status,
        calculation->minutesDifference,
        std::move(request.snapshotJpeg)};
    const auto saveResult = [&] {
        try {
            return attendanceRepository_.save(entry);
        } catch (...) {
            return Result<void, storage::RepositoryError>::failure(
                storage::RepositoryError::WriteFailed);
        }
    }();
    if (!saveResult) {
        return Result<PunchReceipt, PunchError>::failure(PunchError::WriteFailed);
    }

    return Result<PunchReceipt, PunchError>::success(PunchReceipt{
        shift.id,
        calculation->status,
        calculation->minutesDifference,
        calculation->checkIn});
}

} // namespace smart_attendance::services

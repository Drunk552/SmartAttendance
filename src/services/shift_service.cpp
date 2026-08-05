#include "shift_service.h"

#include "core/attendance/punch_rule.h"

namespace smart_attendance::services {

bool ShiftService::validPeriod(const std::string& start, const std::string& end) noexcept {
    if (start.empty() && end.empty()) return true;
    const auto first = core::parseTimeToMinutes(start);
    const auto last = core::parseTimeToMinutes(end);
    return first.has_value() && last.has_value() && *first < *last;
}

Result<std::vector<core::Shift>, ShiftError> ShiftService::listShifts() {
    const auto result = repository_.listAll();
    if (!result) return Result<std::vector<core::Shift>, ShiftError>::failure(ShiftError::ReadFailed);
    return Result<std::vector<core::Shift>, ShiftError>::success(result.value());
}

Result<core::Shift, ShiftError> ShiftService::findById(int shiftId) {
    if (shiftId <= 0) return Result<core::Shift, ShiftError>::failure(ShiftError::InvalidShiftId);
    const auto result = repository_.findById(shiftId);
    if (!result) return Result<core::Shift, ShiftError>::failure(ShiftError::ReadFailed);
    if (!result.value()) return Result<core::Shift, ShiftError>::failure(ShiftError::NotFound);
    return Result<core::Shift, ShiftError>::success(*result.value());
}

Result<void, ShiftError> ShiftService::update(const core::Shift& shift) {
    if (shift.id <= 0) return Result<void, ShiftError>::failure(ShiftError::InvalidShiftId);
    if (!validPeriod(shift.firstStart, shift.firstEnd) ||
        !validPeriod(shift.secondStart, shift.secondEnd) ||
        !validPeriod(shift.thirdStart, shift.thirdEnd)) {
        return Result<void, ShiftError>::failure(ShiftError::InvalidTimeRange);
    }
    if (shift.crossDay != 0 && shift.crossDay != 1) {
        return Result<void, ShiftError>::failure(ShiftError::InvalidTimeRange);
    }
    const auto result = repository_.update(shift);
    if (!result) return Result<void, ShiftError>::failure(ShiftError::WriteFailed);
    return Result<void, ShiftError>::success();
}
} // namespace smart_attendance::services

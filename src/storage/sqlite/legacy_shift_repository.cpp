#include "legacy_shift_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {
namespace {
core::Shift mapShift(const ShiftInfo& value) {
    return {value.id, value.name, value.s1_start, value.s1_end, value.s2_start,
            value.s2_end, value.s3_start, value.s3_end, value.cross_day};
}
}

Result<std::vector<core::Shift>, RepositoryError> LegacyShiftRepository::listAll() {
    if (!data_is_open()) {
        return Result<std::vector<core::Shift>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    }
    try {
        const auto source = db_get_shifts();
        std::vector<core::Shift> result;
        result.reserve(source.size() > 10 ? 10 : source.size());
        for (const auto& shift : source) {
            if (result.size() == 10) break;
            result.push_back(mapShift(shift));
        }
        return Result<std::vector<core::Shift>, RepositoryError>::success(std::move(result));
    } catch (...) {
        return Result<std::vector<core::Shift>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    }
}

Result<std::optional<core::Shift>, RepositoryError>
LegacyShiftRepository::findById(int shiftId) {
    using Return = Result<std::optional<core::Shift>, RepositoryError>;
    if (!data_is_open()) return Return::failure(RepositoryError::ReadFailed);
    try {
        const auto source = db_get_shift_info(shiftId);
        if (!source) return Return::success(std::nullopt);
        return Return::success(mapShift(*source));
    } catch (...) {
        return Return::failure(RepositoryError::ReadFailed);
    }
}

Result<void, RepositoryError> LegacyShiftRepository::update(const core::Shift& shift) {
    if (!data_is_open()) return Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
    try {
        if (!db_update_shift(shift.id, shift.firstStart, shift.firstEnd, shift.secondStart,
                             shift.secondEnd, shift.thirdStart, shift.thirdEnd,
                             shift.crossDay)) {
            return Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
        }
        return Result<void, RepositoryError>::success();
    } catch (...) {
        return Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
    }
}
} // namespace smart_attendance::storage::sqlite

#ifndef SMART_ATTENDANCE_SERVICES_SHIFT_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_SHIFT_SERVICE_H

#include "core/model/shift.h"
#include "core/common/result.h"
#include "storage/repository/shift_repository.h"

#include <optional>
#include <vector>

namespace smart_attendance::services {

enum class ShiftError { InvalidShiftId, InvalidTimeRange, NotFound, ReadFailed, WriteFailed };

class ShiftService final {
public:
    explicit ShiftService(storage::IShiftRepository& repository) noexcept
        : repository_(repository) {}
    Result<std::vector<core::Shift>, ShiftError> listShifts();
    Result<core::Shift, ShiftError> findById(int shiftId);
    Result<void, ShiftError> update(const core::Shift& shift);

private:
    static bool validPeriod(const std::string& start, const std::string& end) noexcept;
    storage::IShiftRepository& repository_;
};
} // namespace smart_attendance::services
#endif

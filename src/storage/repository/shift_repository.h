#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_SHIFT_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_SHIFT_REPOSITORY_H

#include "core/common/result.h"
#include "core/model/shift.h"
#include "storage/repository/repository_error.h"

#include <optional>
#include <vector>

namespace smart_attendance::storage {

class IShiftRepository {
public:
    virtual ~IShiftRepository() = default;
    virtual Result<std::vector<core::Shift>, RepositoryError> listAll() = 0;
    virtual Result<std::optional<core::Shift>, RepositoryError>
    findById(int shiftId) = 0;
    virtual Result<void, RepositoryError> update(const core::Shift& shift) = 0;
};

} // namespace smart_attendance::storage

#endif

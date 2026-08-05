#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SHIFT_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SHIFT_REPOSITORY_H

#include "storage/repository/shift_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyShiftRepository final : public IShiftRepository {
public:
    Result<std::vector<core::Shift>, RepositoryError> listAll() override;
    Result<std::optional<core::Shift>, RepositoryError>
    findById(int shiftId) override;
    Result<void, RepositoryError> update(const core::Shift& shift) override;
};

} // namespace smart_attendance::storage::sqlite

#endif

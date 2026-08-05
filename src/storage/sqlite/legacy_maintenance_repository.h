#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_MAINTENANCE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_MAINTENANCE_REPOSITORY_H

#include "storage/repository/maintenance_repository.h"

namespace smart_attendance::storage::sqlite {
class LegacyMaintenanceRepository final : public IMaintenanceRepository {
public:
    Result<void, RepositoryError> clearAttendance() override;
    Result<void, RepositoryError> clearEmployees() override;
    Result<void, RepositoryError> clearAllData() override;
    Result<void, RepositoryError> factoryReset() override;
};
}
#endif

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_MAINTENANCE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_MAINTENANCE_REPOSITORY_H

#include "core/common/result.h"
#include "storage/repository/repository_error.h"

namespace smart_attendance::storage {
class IMaintenanceRepository {
public:
    virtual ~IMaintenanceRepository() = default;
    virtual Result<void, RepositoryError> clearAttendance() = 0;
    virtual Result<void, RepositoryError> clearEmployees() = 0;
    virtual Result<void, RepositoryError> clearAllData() = 0;
    virtual Result<void, RepositoryError> factoryReset() = 0;
};
}
#endif

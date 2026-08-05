#ifndef SMART_ATTENDANCE_SERVICES_MAINTENANCE_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_MAINTENANCE_SERVICE_H

#include "storage/repository/maintenance_repository.h"

namespace smart_attendance::services {
enum class MaintenanceError { WriteFailed };
class MaintenanceService final {
public:
    explicit MaintenanceService(storage::IMaintenanceRepository& repository) noexcept : repository_(repository) {}
    Result<void, MaintenanceError> clearAttendance();
    Result<void, MaintenanceError> clearEmployees();
    Result<void, MaintenanceError> clearAllData();
    Result<void, MaintenanceError> factoryReset();
private:
    Result<void, MaintenanceError> map(Result<void, storage::RepositoryError> result);
    storage::IMaintenanceRepository& repository_;
};
}
#endif

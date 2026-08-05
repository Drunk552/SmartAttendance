#include "maintenance_service.h"

namespace smart_attendance::services {
Result<void, MaintenanceError> MaintenanceService::map(
    Result<void, storage::RepositoryError> result) {
    return result ? Result<void, MaintenanceError>::success()
                  : Result<void, MaintenanceError>::failure(MaintenanceError::WriteFailed);
}
Result<void, MaintenanceError> MaintenanceService::clearAttendance() { return map(repository_.clearAttendance()); }
Result<void, MaintenanceError> MaintenanceService::clearEmployees() { return map(repository_.clearEmployees()); }
Result<void, MaintenanceError> MaintenanceService::clearAllData() { return map(repository_.clearAllData()); }
Result<void, MaintenanceError> MaintenanceService::factoryReset() { return map(repository_.factoryReset()); }
}

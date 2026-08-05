#include "legacy_maintenance_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {
namespace {
Result<void, RepositoryError> writeResult(bool success) {
    return success ? Result<void, RepositoryError>::success()
                   : Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
}
}
Result<void, RepositoryError> LegacyMaintenanceRepository::clearAttendance() {
    return data_is_open() ? writeResult(db_clear_attendance())
                          : writeResult(false);
}
Result<void, RepositoryError> LegacyMaintenanceRepository::clearEmployees() {
    return data_is_open() ? writeResult(db_clear_all_employee_data(false))
                          : writeResult(false);
}
Result<void, RepositoryError> LegacyMaintenanceRepository::clearAllData() {
    return data_is_open() ? writeResult(db_clear_all_employee_data(false))
                          : writeResult(false);
}
Result<void, RepositoryError> LegacyMaintenanceRepository::factoryReset() {
    return data_is_open() ? writeResult(db_factory_reset()) : writeResult(false);
}
}

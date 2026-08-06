#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_EMPLOYEE_SETTINGS_IMPORT_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_EMPLOYEE_SETTINGS_IMPORT_REPOSITORY_H

#include "storage/repository/employee_settings_import_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyEmployeeSettingsImportRepository final
    : public IEmployeeSettingsImportRepository {
public:
    bool updateShift(
        int shiftId,
        const std::string& firstStart,
        const std::string& firstEnd,
        const std::string& secondStart,
        const std::string& secondEnd,
        const std::string& thirdStart,
        const std::string& thirdEnd) override;
    bool importUsers(
        int year, int month,
        const std::vector<EmployeeSettingsImportUser>& users) override;
};

} // namespace smart_attendance::storage::sqlite

#endif

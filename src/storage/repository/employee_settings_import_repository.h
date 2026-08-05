#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_EMPLOYEE_SETTINGS_IMPORT_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_EMPLOYEE_SETTINGS_IMPORT_REPOSITORY_H

#include "data/db_storage.h"

namespace smart_attendance::storage {

class IEmployeeSettingsImportRepository {
public:
    virtual ~IEmployeeSettingsImportRepository() = default;
    virtual bool updateShift(
        int shiftId,
        const std::string& firstStart,
        const std::string& firstEnd,
        const std::string& secondStart,
        const std::string& secondEnd,
        const std::string& thirdStart,
        const std::string& thirdEnd) = 0;
    virtual bool importUsers(
        int year, int month, const std::vector<UserData>& users) = 0;
};

} // namespace smart_attendance::storage

#endif

#include "legacy_employee_settings_import_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

bool LegacyEmployeeSettingsImportRepository::updateShift(
    int shiftId,
    const std::string& firstStart,
    const std::string& firstEnd,
    const std::string& secondStart,
    const std::string& secondEnd,
    const std::string& thirdStart,
    const std::string& thirdEnd) {
    return db_update_shift(
        shiftId, firstStart, firstEnd, secondStart, secondEnd,
        thirdStart, thirdEnd, 0);
}

bool LegacyEmployeeSettingsImportRepository::importUsers(
    int year, int month, const std::vector<UserData>& users) {
    if (!db_batch_add_users(users)) {
        return false;
    }
    (void)db_batch_update_user_schedules(year, month, users);
    return true;
}

} // namespace smart_attendance::storage::sqlite

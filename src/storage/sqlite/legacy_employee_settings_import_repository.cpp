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
    int year, int month,
    const std::vector<EmployeeSettingsImportUser>& users) {
    std::vector<UserData> legacyUsers;
    legacyUsers.reserve(users.size());
    for (const auto& user : users) {
        UserData legacy;
        legacy.id = user.id;
        legacy.name = user.name;
        legacy.dept_id = user.departmentId;
        legacy.role = user.role;
        legacy.monthly_schedule = user.monthlySchedule;
        legacyUsers.push_back(std::move(legacy));
    }
    if (!db_batch_add_users(legacyUsers)) {
        return false;
    }
    (void)db_batch_update_user_schedules(year, month, legacyUsers);
    return true;
}

} // namespace smart_attendance::storage::sqlite

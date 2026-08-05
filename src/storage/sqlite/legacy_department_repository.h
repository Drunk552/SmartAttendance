/**
 * @file legacy_department_repository.h
 * @brief 声明旧 SQLite 部门维护和排班读取 DAO 的适配器。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_DEPARTMENT_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_DEPARTMENT_REPOSITORY_H

#include "storage/repository/department_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyDepartmentRepository final : public IDepartmentRepository {
public:
    Result<void, RepositoryError>
    addDefaultCompanyDepartment(const std::string& name) override;

    Result<void, RepositoryError>
    renameDepartment(int departmentId, const std::string& name) override;

    Result<void, RepositoryError>
    removeDepartment(int departmentId) override;

    Result<void, RepositoryError> updateSchedule(
        int departmentId,
        const std::string& departmentName,
        const std::array<int, 7>& shiftIds) override;

    Result<std::vector<core::Department>, RepositoryError>
    listDefaultCompany() override;

    Result<int, RepositoryError>
    countEmployees(int departmentId) override;

    Result<std::optional<core::DepartmentSchedule>, RepositoryError>
    findSchedule(int departmentId) override;
};

} // namespace smart_attendance::storage::sqlite

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_DEPARTMENT_REPOSITORY_H

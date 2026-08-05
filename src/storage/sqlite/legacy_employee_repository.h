/**
 * @file legacy_employee_repository.h
 * @brief 声明将现有 db_storage 接入员工仓储抽象的过渡适配器。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_EMPLOYEE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_EMPLOYEE_REPOSITORY_H

#include "storage/repository/employee_repository.h"

namespace smart_attendance::storage::sqlite {

/**
 * @brief 通过进程级 db_storage 查询员工的过渡适配器。
 * @note Application 必须保证调用期间数据库保持打开。
 */
class LegacyEmployeeRepository final : public IEmployeeRepository {
public:
    Result<std::optional<core::Employee>, RepositoryError>
    findById(int employeeId) override;

    Result<std::optional<EmployeeDisplayDetails>, RepositoryError>
    findDisplayDetailsById(int employeeId) override;

    Result<void, RepositoryError>
    updateName(int employeeId, const std::string& name) override;

    Result<void, RepositoryError>
    updateDepartment(int employeeId, int departmentId) override;

    Result<void, RepositoryError>
    updateRole(int employeeId, int role) override;

    Result<PasswordVerification, RepositoryError>
    verifyPassword(int employeeId, const std::string& password) override;

    Result<bool, RepositoryError>
    updatePassword(int employeeId, const std::string& password) override;

    Result<bool, RepositoryError> remove(int employeeId) override;

    Result<EmployeePage, RepositoryError>
    listPage(std::size_t offset, std::size_t limit) override;
};

} // namespace smart_attendance::storage::sqlite

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_EMPLOYEE_REPOSITORY_H

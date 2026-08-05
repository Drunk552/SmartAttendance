/**
 * @file department_repository.h
 * @brief 声明部门维护和部门排班读取的存储抽象。
 */

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_DEPARTMENT_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_DEPARTMENT_REPOSITORY_H

#include "core/common/result.h"
#include "core/model/department.h"
#include "storage/repository/repository_error.h"

#include <optional>
#include <array>
#include <string>
#include <vector>

namespace smart_attendance::storage {

class IDepartmentRepository {
public:
    virtual ~IDepartmentRepository() = default;

    virtual Result<void, RepositoryError>
    addDefaultCompanyDepartment(const std::string& name) = 0;

    virtual Result<void, RepositoryError>
    renameDepartment(int departmentId, const std::string& name) = 0;

    virtual Result<void, RepositoryError>
    removeDepartment(int departmentId) = 0;

    virtual Result<void, RepositoryError> updateSchedule(
        int departmentId,
        const std::string& departmentName,
        const std::array<int, 7>& shiftIds) = 0;

    virtual Result<std::vector<core::Department>, RepositoryError>
    listDefaultCompany() = 0;

    virtual Result<int, RepositoryError>
    countEmployees(int departmentId) = 0;

    virtual Result<std::optional<core::DepartmentSchedule>, RepositoryError>
    findSchedule(int departmentId) = 0;
};

} // namespace smart_attendance::storage

#endif // SMART_ATTENDANCE_STORAGE_REPOSITORY_DEPARTMENT_REPOSITORY_H

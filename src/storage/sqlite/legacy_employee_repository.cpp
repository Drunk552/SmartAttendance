/**
 * @file legacy_employee_repository.cpp
 * @brief 实现 db_storage 员工数据到领域模型的过渡映射。
 */

#include "legacy_employee_repository.h"

#include "data/db_storage.h"

#include <utility>

namespace smart_attendance::storage::sqlite {

static_assert(kMaxEmployeePageSize == kMaxDbUserBasicPageSize,
              "employee page limits must remain aligned across the adapter boundary");

Result<std::optional<core::Employee>, RepositoryError>
LegacyEmployeeRepository::findById(int employeeId) {
    using ResultType = Result<std::optional<core::Employee>, RepositoryError>;
    try {
        DbUserLookupResult lookup = db_find_user_info(employeeId);
        if (lookup.status == DbUserLookupStatus::ReadError) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }
        if (lookup.status == DbUserLookupStatus::NotFound) {
            return ResultType::success(std::nullopt);
        }

        const auto& user = lookup.user;
        if (!user) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }

        core::EmployeeRole role;
        if (user->role == 0) {
            role = core::EmployeeRole::Regular;
        } else if (user->role == 1) {
            role = core::EmployeeRole::Administrator;
        } else {
            return ResultType::failure(RepositoryError::ReadFailed);
        }

        return ResultType::success(core::Employee{
            user->id,
            user->name,
            user->dept_id,
            role});
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

Result<EmployeePage, RepositoryError>
LegacyEmployeeRepository::listPage(std::size_t offset, std::size_t limit) {
    using ResultType = Result<EmployeePage, RepositoryError>;
    if (limit == 0 || limit > kMaxEmployeePageSize) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }

    try {
        DbUserPageResult page = db_find_user_basics_page(offset, limit);
        if (page.status == DbUserPageStatus::InvalidArgument) {
            return ResultType::failure(RepositoryError::InvalidArgument);
        }
        if (page.status == DbUserPageStatus::ReadError) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }

        std::vector<core::Employee> employees;
        employees.reserve(page.users.size());
        for (const UserData& user : page.users) {
            core::EmployeeRole role;
            if (user.role == 0) {
                role = core::EmployeeRole::Regular;
            } else if (user.role == 1) {
                role = core::EmployeeRole::Administrator;
            } else {
                return ResultType::failure(RepositoryError::ReadFailed);
            }
            employees.push_back(core::Employee{
                user.id,
                user.name,
                user.dept_id,
                role});
        }

        return ResultType::success(EmployeePage{
            std::move(employees),
            page.has_more});
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

} // namespace smart_attendance::storage::sqlite

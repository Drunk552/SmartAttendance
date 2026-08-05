/**
 * @file employee_service.cpp
 * @brief 实现员工查询用例服务。
 */

#include "employee_service.h"

#include <utility>

namespace smart_attendance::services {

EmployeeService::EmployeeService(storage::IEmployeeRepository& repository) noexcept
    : repository_(repository) {}

Result<std::optional<core::Employee>, EmployeeError>
EmployeeService::findById(int employeeId) {
    using ResultType = Result<std::optional<core::Employee>, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }

    try {
        auto result = repository_.findById(employeeId);
        if (!result) {
            return ResultType::failure(EmployeeError::ReadFailed);
        }
        return ResultType::success(result.value());
    } catch (...) {
        return ResultType::failure(EmployeeError::ReadFailed);
    }
}

Result<EmployeePage, EmployeeError>
EmployeeService::listPage(std::size_t offset, std::size_t limit) {
    using ResultType = Result<EmployeePage, EmployeeError>;
    if (limit == 0 || limit > storage::kMaxEmployeePageSize) {
        return ResultType::failure(EmployeeError::InvalidPageRequest);
    }

    try {
        auto result = repository_.listPage(offset, limit);
        if (!result) {
            return ResultType::failure(
                result.error() == storage::RepositoryError::InvalidArgument
                    ? EmployeeError::InvalidPageRequest
                    : EmployeeError::ReadFailed);
        }

        storage::EmployeePage page = std::move(result).value();
        return ResultType::success(EmployeePage{
            std::move(page.employees),
            page.hasMore});
    } catch (...) {
        return ResultType::failure(EmployeeError::ReadFailed);
    }
}

} // namespace smart_attendance::services

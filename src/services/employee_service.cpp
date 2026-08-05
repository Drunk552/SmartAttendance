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

Result<std::optional<EmployeeDisplayDetails>, EmployeeError>
EmployeeService::findDisplayDetailsById(int employeeId) {
    using ResultType =
        Result<std::optional<EmployeeDisplayDetails>, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }

    try {
        auto result = repository_.findDisplayDetailsById(employeeId);
        if (!result) {
            return ResultType::failure(EmployeeError::ReadFailed);
        }
        if (!result.value()) {
            return ResultType::success(std::nullopt);
        }

        const auto& details = *result.value();
        return ResultType::success(EmployeeDisplayDetails{
            details.employee,
            details.departmentName,
            details.faceRegistered,
            details.fingerprintRegistered,
            details.cardId,
            details.passwordRegistered});
    } catch (...) {
        return ResultType::failure(EmployeeError::ReadFailed);
    }
}

Result<void, EmployeeError> EmployeeService::updateName(
    int employeeId, const std::string& name) {
    using ResultType = Result<void, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }
    if (name.empty()) {
        return ResultType::failure(EmployeeError::InvalidName);
    }
    try {
        auto result = repository_.updateName(employeeId, name);
        if (!result) {
            return ResultType::failure(
                result.error() == storage::RepositoryError::InvalidArgument
                    ? EmployeeError::InvalidName
                    : EmployeeError::WriteFailed);
        }
        return ResultType::success();
    } catch (...) {
        return ResultType::failure(EmployeeError::WriteFailed);
    }
}

Result<void, EmployeeError> EmployeeService::updateDepartment(
    int employeeId, int departmentId) {
    using ResultType = Result<void, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }
    if (departmentId <= 0) {
        return ResultType::failure(EmployeeError::InvalidDepartmentId);
    }
    try {
        auto result = repository_.updateDepartment(employeeId, departmentId);
        if (!result) {
            return ResultType::failure(
                result.error() == storage::RepositoryError::InvalidArgument
                    ? EmployeeError::InvalidDepartmentId
                    : EmployeeError::WriteFailed);
        }
        return ResultType::success();
    } catch (...) {
        return ResultType::failure(EmployeeError::WriteFailed);
    }
}

Result<void, EmployeeError> EmployeeService::updateRole(
    int employeeId, int role) {
    using ResultType = Result<void, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }
    if (role != 0 && role != 1) {
        return ResultType::failure(EmployeeError::InvalidRole);
    }
    try {
        auto result = repository_.updateRole(employeeId, role);
        if (!result) {
            return ResultType::failure(
                result.error() == storage::RepositoryError::InvalidArgument
                    ? EmployeeError::InvalidRole
                    : EmployeeError::WriteFailed);
        }
        return ResultType::success();
    } catch (...) {
        return ResultType::failure(EmployeeError::WriteFailed);
    }
}

Result<PasswordVerification, EmployeeError> EmployeeService::verifyPassword(
    int employeeId, const std::string& password) {
    using ResultType = Result<PasswordVerification, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }
    if (password.empty() || password.size() > 6) {
        return ResultType::failure(EmployeeError::InvalidPassword);
    }
    try {
        auto result = repository_.verifyPassword(employeeId, password);
        if (!result) {
            return ResultType::failure(EmployeeError::ReadFailed);
        }
        switch (result.value()) {
        case storage::PasswordVerification::Match:
            return ResultType::success(PasswordVerification::Match);
        case storage::PasswordVerification::Mismatch:
            return ResultType::success(PasswordVerification::Mismatch);
        case storage::PasswordVerification::NotConfigured:
            return ResultType::success(PasswordVerification::NotConfigured);
        case storage::PasswordVerification::NotFound:
            return ResultType::success(PasswordVerification::NotFound);
        }
    } catch (...) {
    }
    return ResultType::failure(EmployeeError::ReadFailed);
}

Result<void, EmployeeError> EmployeeService::updatePassword(
    int employeeId, const std::string& password) {
    using ResultType = Result<void, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }
    if (password.empty() || password.size() > 6) {
        return ResultType::failure(EmployeeError::InvalidPassword);
    }
    try {
        auto result = repository_.updatePassword(employeeId, password);
        if (!result) {
            return ResultType::failure(EmployeeError::WriteFailed);
        }
        if (!result.value()) {
            return ResultType::failure(EmployeeError::NotFound);
        }
        return ResultType::success();
    } catch (...) {
        return ResultType::failure(EmployeeError::WriteFailed);
    }
}

Result<void, EmployeeError> EmployeeService::remove(int employeeId) {
    using ResultType = Result<void, EmployeeError>;
    if (employeeId <= 0) {
        return ResultType::failure(EmployeeError::InvalidEmployeeId);
    }
    try {
        auto result = repository_.remove(employeeId);
        if (!result) {
            return ResultType::failure(EmployeeError::WriteFailed);
        }
        if (!result.value()) {
            return ResultType::failure(EmployeeError::NotFound);
        }
        return ResultType::success();
    } catch (...) {
        return ResultType::failure(EmployeeError::WriteFailed);
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
        std::vector<EmployeePage::Entry> employees;
        employees.reserve(page.employees.size());
        for (auto& entry : page.employees) {
            employees.push_back(EmployeePage::Entry{
                std::move(entry.employee),
                std::move(entry.departmentName)});
        }
        return ResultType::success(
            EmployeePage{std::move(employees), page.hasMore});
    } catch (...) {
        return ResultType::failure(EmployeeError::ReadFailed);
    }
}

} // namespace smart_attendance::services

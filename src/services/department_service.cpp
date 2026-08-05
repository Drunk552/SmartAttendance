/**
 * @file department_service.cpp
 * @brief 实现部门维护和排班读取用例。
 */

#include "department_service.h"

namespace smart_attendance::services {

DepartmentService::DepartmentService(
    storage::IDepartmentRepository& repository) noexcept
    : repository_(repository) {}

Result<void, DepartmentError>
DepartmentService::addDepartment(const std::string& name) {
    using ResultType = Result<void, DepartmentError>;
    if (name.empty()) {
        return ResultType::failure(DepartmentError::InvalidDepartmentName);
    }
    if (!repository_.addDefaultCompanyDepartment(name)) {
        return ResultType::failure(DepartmentError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, DepartmentError>
DepartmentService::renameDepartment(
    int departmentId, const std::string& name) {
    using ResultType = Result<void, DepartmentError>;
    if (departmentId <= 0) {
        return ResultType::failure(DepartmentError::InvalidDepartmentId);
    }
    if (name.empty()) {
        return ResultType::failure(DepartmentError::InvalidDepartmentName);
    }
    if (!repository_.renameDepartment(departmentId, name)) {
        return ResultType::failure(DepartmentError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, DepartmentError>
DepartmentService::removeDepartment(int departmentId) {
    using ResultType = Result<void, DepartmentError>;
    if (departmentId <= 0) {
        return ResultType::failure(DepartmentError::InvalidDepartmentId);
    }
    const auto count = repository_.countEmployees(departmentId);
    if (!count) {
        return ResultType::failure(DepartmentError::ReadFailed);
    }
    if (count.value() > 0) {
        return ResultType::failure(DepartmentError::HasEmployees);
    }
    if (!repository_.removeDepartment(departmentId)) {
        return ResultType::failure(DepartmentError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, DepartmentError> DepartmentService::updateSchedule(
    int departmentId,
    const std::string& departmentName,
    const std::array<int, 7>& shiftIds) {
    using ResultType = Result<void, DepartmentError>;
    if (departmentId <= 0) {
        return ResultType::failure(DepartmentError::InvalidDepartmentId);
    }
    if (departmentName.empty()) {
        return ResultType::failure(DepartmentError::InvalidDepartmentName);
    }
    for (int shiftId : shiftIds) {
        if (shiftId < 0 || shiftId > 10) {
            return ResultType::failure(DepartmentError::InvalidShiftId);
        }
    }
    if (!repository_.updateSchedule(departmentId, departmentName, shiftIds)) {
        return ResultType::failure(DepartmentError::WriteFailed);
    }
    return ResultType::success();
}

Result<std::vector<core::Department>, DepartmentError>
DepartmentService::listDepartments() {
    using ResultType = Result<std::vector<core::Department>, DepartmentError>;
    const auto result = repository_.listDefaultCompany();
    if (!result) {
        return ResultType::failure(DepartmentError::ReadFailed);
    }
    return ResultType::success(result.value());
}

Result<int, DepartmentError>
DepartmentService::employeeCount(int departmentId) {
    using ResultType = Result<int, DepartmentError>;
    if (departmentId <= 0) {
        return ResultType::failure(DepartmentError::InvalidDepartmentId);
    }
    const auto result = repository_.countEmployees(departmentId);
    if (!result) {
        return ResultType::failure(DepartmentError::ReadFailed);
    }
    return ResultType::success(result.value());
}

Result<std::optional<core::DepartmentSchedule>, DepartmentError>
DepartmentService::findSchedule(int departmentId) {
    using ResultType =
        Result<std::optional<core::DepartmentSchedule>, DepartmentError>;
    if (departmentId <= 0) {
        return ResultType::failure(DepartmentError::InvalidDepartmentId);
    }
    const auto result = repository_.findSchedule(departmentId);
    if (!result) {
        return ResultType::failure(DepartmentError::ReadFailed);
    }
    return ResultType::success(result.value());
}

} // namespace smart_attendance::services

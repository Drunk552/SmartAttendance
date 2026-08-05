/**
 * @file department_service.h
 * @brief 声明部门维护和排班读取用例服务。
 */

#ifndef SMART_ATTENDANCE_SERVICES_DEPARTMENT_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_DEPARTMENT_SERVICE_H

#include "core/common/result.h"
#include "core/model/department.h"
#include "storage/repository/department_repository.h"

#include <optional>
#include <array>
#include <string>
#include <vector>

namespace smart_attendance::services {

enum class DepartmentError {
    InvalidDepartmentId,
    InvalidDepartmentName,
    HasEmployees,
    InvalidShiftId,
    ReadFailed,
    WriteFailed
};

class DepartmentService final {
public:
    explicit DepartmentService(storage::IDepartmentRepository& repository) noexcept;

    Result<std::vector<core::Department>, DepartmentError> listDepartments();
    Result<void, DepartmentError> addDepartment(const std::string& name);
    Result<void, DepartmentError>
    renameDepartment(int departmentId, const std::string& name);
    Result<void, DepartmentError> removeDepartment(int departmentId);
    Result<void, DepartmentError> updateSchedule(
        int departmentId,
        const std::string& departmentName,
        const std::array<int, 7>& shiftIds);
    Result<int, DepartmentError> employeeCount(int departmentId);
    Result<std::optional<core::DepartmentSchedule>, DepartmentError>
    findSchedule(int departmentId);

private:
    storage::IDepartmentRepository& repository_;
};

} // namespace smart_attendance::services

#endif // SMART_ATTENDANCE_SERVICES_DEPARTMENT_SERVICE_H

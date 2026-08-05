/**
 * @file department_presenter.cpp
 * @brief 实现部门设置页面的状态和命令映射。
 */

#include "department_presenter.h"

namespace smart_attendance::ui {

DepartmentPresenter::DepartmentPresenter(
    services::DepartmentService& service) noexcept
    : service_(service) {}

bool DepartmentPresenter::listDepartments(
    std::vector<DepartmentItem>& departments) {
    const auto result = service_.listDepartments();
    if (!result) {
        return false;
    }

    departments.clear();
    departments.reserve(result.value().size());
    for (const auto& department : result.value()) {
        departments.push_back(
            DepartmentItem{
                department.id, department.name, department.companyId});
    }
    return true;
}

bool DepartmentPresenter::employeeCount(int departmentId, int& count) {
    const auto result = service_.employeeCount(departmentId);
    if (!result) {
        return false;
    }
    count = result.value();
    return true;
}

bool DepartmentPresenter::addDepartment(const std::string& name) {
    return static_cast<bool>(service_.addDepartment(name));
}

bool DepartmentPresenter::renameDepartment(
    int departmentId, const std::string& name) {
    return static_cast<bool>(service_.renameDepartment(departmentId, name));
}

bool DepartmentPresenter::removeDepartment(int departmentId) {
    return static_cast<bool>(service_.removeDepartment(departmentId));
}

bool DepartmentPresenter::updateSchedule(
    int departmentId,
    const std::string& departmentName,
    const std::vector<int>& shiftIds) {
    if (shiftIds.size() != 7) {
        return false;
    }
    std::array<int, 7> fixedShiftIds{};
    for (std::size_t index = 0; index < fixedShiftIds.size(); ++index) {
        fixedShiftIds[index] = shiftIds[index];
    }
    return static_cast<bool>(
        service_.updateSchedule(departmentId, departmentName, fixedShiftIds));
}

bool DepartmentPresenter::loadSchedule(
    int departmentId, DepartmentScheduleState& schedule) {
    const auto result = service_.findSchedule(departmentId);
    if (!result || !result.value()) {
        return false;
    }

    const auto& value = *result.value();
    schedule = DepartmentScheduleState{
        value.departmentId, value.departmentName, value.shiftIds};
    return true;
}

} // namespace smart_attendance::ui

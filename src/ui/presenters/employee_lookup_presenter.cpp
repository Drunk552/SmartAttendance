/**
 * @file employee_lookup_presenter.cpp
 * @brief 实现员工查询的旧 UI 返回值映射。
 */

#include "employee_lookup_presenter.h"

namespace smart_attendance::ui {

EmployeeLookupPresenter::EmployeeLookupPresenter(
    services::EmployeeService& service) noexcept
    : service_(service) {}

int EmployeeLookupPresenter::roleValueById(int employeeId) {
    const auto result = service_.findById(employeeId);
    if (!result || !result.value()) {
        return -1;
    }

    return result.value()->role == core::EmployeeRole::Administrator ? 1 : 0;
}

bool EmployeeLookupPresenter::existsById(int employeeId) {
    const auto result = service_.findById(employeeId);
    return result && result.value().has_value();
}

} // namespace smart_attendance::ui

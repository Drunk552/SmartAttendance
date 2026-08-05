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

std::optional<EmployeeLookupPresenter::DisplayDetails>
EmployeeLookupPresenter::findDisplayDetailsById(int employeeId) {
    const auto result = service_.findDisplayDetailsById(employeeId);
    if (!result || !result.value()) {
        return std::nullopt;
    }

    const auto& details = *result.value();
    return DisplayDetails{
        details.employee.id,
        details.employee.name,
        details.employee.departmentId,
        details.departmentName,
        details.faceRegistered,
        details.fingerprintRegistered,
        details.cardId,
        details.passwordRegistered,
        details.employee.role == core::EmployeeRole::Administrator ? 1 : 0};
}

bool EmployeeLookupPresenter::updateName(
    int employeeId, const std::string& name) {
    return service_.updateName(employeeId, name).hasValue();
}

bool EmployeeLookupPresenter::updateDepartment(int employeeId, int departmentId) {
    return service_.updateDepartment(employeeId, departmentId).hasValue();
}

bool EmployeeLookupPresenter::updateRole(int employeeId, int role) {
    return service_.updateRole(employeeId, role).hasValue();
}

bool EmployeeLookupPresenter::verifyPassword(
    int employeeId, const std::string& password) {
    const auto result = service_.verifyPassword(employeeId, password);
    return result && result.value() == services::PasswordVerification::Match;
}

bool EmployeeLookupPresenter::updatePassword(
    int employeeId, const std::string& password) {
    return service_.updatePassword(employeeId, password).hasValue();
}

bool EmployeeLookupPresenter::remove(int employeeId) {
    return service_.remove(employeeId).hasValue();
}

std::vector<EmployeeLookupPresenter::ListItem>
EmployeeLookupPresenter::listAll() {
    std::vector<ListItem> items;
    std::size_t offset = 0;

    while (true) {
        auto result = service_.listPage(
            offset, storage::kMaxEmployeePageSize);
        if (!result) {
            return {};
        }

        services::EmployeePage page = std::move(result).value();
        items.reserve(items.size() + page.employees.size());
        for (auto& entry : page.employees) {
            const auto& employee = entry.employee;
            items.push_back(ListItem{
                employee.id,
                std::move(entry.employee.name),
                employee.departmentId,
                std::move(entry.departmentName),
                employee.role == core::EmployeeRole::Administrator ? 1 : 0});
        }

        if (!page.hasMore) {
            return items;
        }
        if (page.employees.empty()) {
            return {};
        }
        offset += page.employees.size();
    }
}

} // namespace smart_attendance::ui

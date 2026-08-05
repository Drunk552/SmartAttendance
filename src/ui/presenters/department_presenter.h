/**
 * @file department_presenter.h
 * @brief 声明部门设置页面的 Presenter。
 */

#ifndef SMART_ATTENDANCE_UI_PRESENTERS_DEPARTMENT_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_DEPARTMENT_PRESENTER_H

#include "services/department_service.h"

#include <array>
#include <string>
#include <vector>

namespace smart_attendance::ui {

struct DepartmentItem {
    int id;
    std::string name;
    int companyId;
};

struct DepartmentScheduleState {
    int departmentId;
    std::string departmentName;
    std::array<int, 7> shiftIds;
};

class DepartmentPresenter final {
public:
    explicit DepartmentPresenter(services::DepartmentService& service) noexcept;

    bool listDepartments(std::vector<DepartmentItem>& departments);
    bool employeeCount(int departmentId, int& count);
    bool loadSchedule(int departmentId, DepartmentScheduleState& schedule);
    bool addDepartment(const std::string& name);
    bool renameDepartment(int departmentId, const std::string& name);
    bool removeDepartment(int departmentId);
    bool updateSchedule(
        int departmentId,
        const std::string& departmentName,
        const std::vector<int>& shiftIds);

private:
    services::DepartmentService& service_;
};

} // namespace smart_attendance::ui

#endif // SMART_ATTENDANCE_UI_PRESENTERS_DEPARTMENT_PRESENTER_H

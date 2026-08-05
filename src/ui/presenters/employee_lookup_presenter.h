/**
 * @file employee_lookup_presenter.h
 * @brief 声明员工只读查询的轻量 UI 适配器。
 */

#ifndef SMART_ATTENDANCE_UI_PRESENTERS_EMPLOYEE_LOOKUP_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_EMPLOYEE_LOOKUP_PRESENTER_H

#include "services/employee_service.h"

namespace smart_attendance::ui {

/**
 * @brief 将员工查询结果映射为旧 UI 使用的角色和存在性返回值。
 * @note 不持有 LVGL 控件或 Repository；同步调用可能阻塞数据库 IO。
 */
class EmployeeLookupPresenter final {
public:
    explicit EmployeeLookupPresenter(services::EmployeeService& service) noexcept;

    /** @return 普通员工返回 0，管理员返回 1，非法工号、未找到或读取失败返回 -1。 */
    int roleValueById(int employeeId);

    /** @return 员工存在返回 true；非法工号、未找到或读取失败返回 false。 */
    bool existsById(int employeeId);

private:
    services::EmployeeService& service_;
};

} // namespace smart_attendance::ui

#endif // SMART_ATTENDANCE_UI_PRESENTERS_EMPLOYEE_LOOKUP_PRESENTER_H

/**
 * @file employee.h
 * @brief 定义不依赖存储和 UI 的员工基础模型。
 */

#ifndef SMART_ATTENDANCE_CORE_MODEL_EMPLOYEE_H
#define SMART_ATTENDANCE_CORE_MODEL_EMPLOYEE_H

#include <string>

namespace smart_attendance::core {

enum class EmployeeRole {
    Regular,
    Administrator
};

struct Employee {
    int id;
    std::string name;
    int departmentId;
    EmployeeRole role;
};

} // namespace smart_attendance::core

#endif // SMART_ATTENDANCE_CORE_MODEL_EMPLOYEE_H

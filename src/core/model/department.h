/**
 * @file department.h
 * @brief 定义与存储和 UI 无关的部门模型。
 */

#ifndef SMART_ATTENDANCE_CORE_MODEL_DEPARTMENT_H
#define SMART_ATTENDANCE_CORE_MODEL_DEPARTMENT_H

#include <array>
#include <string>

namespace smart_attendance::core {

struct Department {
    int id;
    std::string name;
    int companyId;
};

struct DepartmentSchedule {
    int departmentId;
    std::string departmentName;
    std::array<int, 7> shiftIds;
};

} // namespace smart_attendance::core

#endif // SMART_ATTENDANCE_CORE_MODEL_DEPARTMENT_H

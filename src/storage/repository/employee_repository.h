/**
 * @file employee_repository.h
 * @brief 声明不暴露 SQLite 或生物特征数据的员工查询抽象。
 */

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_EMPLOYEE_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_EMPLOYEE_REPOSITORY_H

#include "core/common/result.h"
#include "core/model/employee.h"
#include "storage/repository/repository_error.h"

#include <optional>
#include <cstddef>
#include <vector>

namespace smart_attendance::storage {

constexpr std::size_t kMaxEmployeePageSize = 64;

struct EmployeePage {
    std::vector<core::Employee> employees;
    bool hasMore;
};

class IEmployeeRepository {
public:
    virtual ~IEmployeeRepository() = default;

    /**
     * @brief 按工号查询员工基础信息。
     * @return 查询成功但员工不存在时返回空 optional；存储读取失败时返回错误。
     * @note 调用可能阻塞数据库 IO；返回值不包含密码或生物特征。
     */
    virtual Result<std::optional<core::Employee>, RepositoryError>
    findById(int employeeId) = 0;

    /**
     * @brief 按工号升序读取一页员工基础信息。
     * @param offset 从零开始的结果偏移量。
     * @param limit 本页上限，必须在 1..kMaxEmployeePageSize 范围内。
     * @return 成功时返回有界结果及是否存在下一页；参数非法或读取失败时返回错误。
     * @note 调用可能阻塞数据库 IO；返回值不包含密码或生物特征。
     */
    virtual Result<EmployeePage, RepositoryError>
    listPage(std::size_t offset, std::size_t limit) = 0;
};

} // namespace smart_attendance::storage

#endif // SMART_ATTENDANCE_STORAGE_REPOSITORY_EMPLOYEE_REPOSITORY_H

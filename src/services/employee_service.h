/**
 * @file employee_service.h
 * @brief 声明员工查询用例服务。
 */

#ifndef SMART_ATTENDANCE_SERVICES_EMPLOYEE_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_EMPLOYEE_SERVICE_H

#include "core/common/result.h"
#include "core/model/employee.h"
#include "storage/repository/employee_repository.h"

#include <optional>
#include <cstddef>
#include <vector>

namespace smart_attendance::services {

enum class EmployeeError {
    InvalidEmployeeId,
    InvalidPageRequest,
    ReadFailed
};

struct EmployeePage {
    std::vector<core::Employee> employees;
    bool hasMore;
};

/**
 * @brief 编排员工基础信息查询，不暴露存储实现或认证资料。
 *
 * 本类不拥有 Repository、不创建线程。调用方必须保证 Repository 在本对象生命周期
 * 内有效。查询是同步阻塞操作，不应直接放在 LVGL 高频刷新回调中执行。
 */
class EmployeeService final {
public:
    explicit EmployeeService(storage::IEmployeeRepository& repository) noexcept;

    /**
     * @brief 按正整数工号查询员工基础信息。
     * @return 员工不存在时成功返回空 optional；存储失败时返回 ReadFailed。
     */
    Result<std::optional<core::Employee>, EmployeeError> findById(int employeeId);

    /**
     * @brief 按工号升序读取一页员工基础信息。
     * @param offset 从零开始的结果偏移量。
     * @param limit 本页上限，必须在 1..storage::kMaxEmployeePageSize 范围内。
     * @return 参数非法、读取失败和成功分页结果具有独立语义。
     */
    Result<EmployeePage, EmployeeError>
    listPage(std::size_t offset, std::size_t limit);

private:
    storage::IEmployeeRepository& repository_;
};

} // namespace smart_attendance::services

#endif // SMART_ATTENDANCE_SERVICES_EMPLOYEE_SERVICE_H

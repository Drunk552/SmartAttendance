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
    struct Entry {
        core::Employee employee;
        std::string departmentName;
    };

    std::vector<Entry> employees;
    bool hasMore;
};

struct EmployeeDisplayDetails {
    core::Employee employee;
    std::string departmentName;
    bool faceRegistered;
    bool fingerprintRegistered;
    std::string cardId;
    bool passwordRegistered;
};

enum class PasswordVerification {
    Match,
    Mismatch,
    NotConfigured,
    NotFound
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
     * @brief 查询详情页展示字段和认证资料存在状态。
     * @note 不返回密码内容、人脸 BLOB 或指纹 BLOB。
     */
    virtual Result<std::optional<EmployeeDisplayDetails>, RepositoryError>
    findDisplayDetailsById(int employeeId) = 0;

    /** @brief 仅更新姓名，不读取或覆盖其他员工资料。 */
    virtual Result<void, RepositoryError>
    updateName(int employeeId, const std::string& name) = 0;

    /** @brief 仅更新所属部门，不读取或覆盖其他员工资料。 */
    virtual Result<void, RepositoryError>
    updateDepartment(int employeeId, int departmentId) = 0;

    /** @brief 仅更新权限等级 0/1，不读取或覆盖其他员工资料。 */
    virtual Result<void, RepositoryError>
    updateRole(int employeeId, int role) = 0;

    /** @brief 验证明文密码，兼容历史明文和现有哈希存储。 */
    virtual Result<PasswordVerification, RepositoryError>
    verifyPassword(int employeeId, const std::string& password) = 0;

    /** @brief 使用现有兼容哈希格式更新密码。 */
    virtual Result<bool, RepositoryError>
    updatePassword(int employeeId, const std::string& password) = 0;

    /**
     * @brief 删除员工数据库记录。
     * @return true 表示删除了一行，false 表示员工不存在。
     * @note 数据库外键负责级联记录；不删除磁盘上的图片文件。
     */
    virtual Result<bool, RepositoryError> remove(int employeeId) = 0;

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

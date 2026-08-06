/**
 * @file employee_lookup_presenter.h
 * @brief 声明员工只读查询的轻量 UI 适配器。
 */

#ifndef SMART_ATTENDANCE_UI_PRESENTERS_EMPLOYEE_LOOKUP_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_EMPLOYEE_LOOKUP_PRESENTER_H

#include "services/employee_service.h"

#include <optional>
#include <string>
#include <vector>

namespace smart_attendance::ui {

/**
 * @brief 将员工查询结果映射为旧 UI 使用的角色和存在性返回值。
 * @note 不持有 LVGL 控件或 Repository；同步调用可能阻塞数据库 IO。
 */
class EmployeeLookupPresenter final {
public:
    struct ListItem {
        int id;
        std::string name;
        int departmentId;
        std::string departmentName;
        int role;
    };

    explicit EmployeeLookupPresenter(services::EmployeeService& service) noexcept;

    /** @return 普通员工返回 0，管理员返回 1，非法工号、未找到或读取失败返回 -1。 */
    int roleValueById(int employeeId);

    /** @return 员工存在返回 true；非法工号、未找到或读取失败返回 false。 */
    bool existsById(int employeeId);

    struct DisplayDetails {
        int id;
        std::string name;
        int departmentId;
        std::string departmentName;
        bool faceRegistered;
        bool fingerprintRegistered;
        std::string cardId;
        bool passwordRegistered;
        int role;
    };

    /** @brief 查询详情页字段，不加载密码或生物特征内容。 */
    std::optional<DisplayDetails> findDisplayDetailsById(int employeeId);

    /** @return true when the basic employee update succeeds. */
    bool updateName(int employeeId, const std::string& name);
    bool updateDepartment(int employeeId, int departmentId);
    bool updateRole(int employeeId, int role);
    bool verifyPassword(int employeeId, const std::string& password);
    bool updatePassword(int employeeId, const std::string& password);
    bool remove(int employeeId);

    /**
     * @brief 读取全部员工列表基础信息，不加载密码或生物特征。
     * @return 读取失败时返回空列表，保持旧 UI 的失败兼容语义。
     */
    std::vector<ListItem> listAll();

    /** @return 当前最大工号加一；没有员工或读取失败时保持旧行为返回 1。 */
    int nextAvailableId();

private:
    services::EmployeeService& service_;
};

} // namespace smart_attendance::ui

#endif // SMART_ATTENDANCE_UI_PRESENTERS_EMPLOYEE_LOOKUP_PRESENTER_H

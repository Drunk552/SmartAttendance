/**
 * @file ui_controller.h
 * @brief UI 控制器头文件 - 提供 UI 层与业务/数据层的接口封装
 * @details 该类封装了 UI 层所需的各种业务逻辑调用，简化 UI 代码复杂度。
 *          实例由 UI 组合层创建，并显式注入各页面模块。
 */
#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <string>
#include <vector>
#include <ctime>
#include <mutex>
#include "ui/presenters/shift_presenter.h"
#include "ui/presenters/attendance_query_presenter.h"
#include "ui/presenters/maintenance_presenter.h"
#include <atomic>

namespace smart_attendance::app {
class UiSystemStatusMailbox;
}

namespace smart_attendance::ui {
class EmployeeLookupPresenter;
class SettingsPresenter;
class DepartmentPresenter;
class SystemInfoPresenter;
}

namespace smart_attendance::hal {
class IRtc;
class IStorageDevice;
}

namespace smart_attendance::storage {
class IEmployeeSettingsImportRepository;
}

// 这里为了简化，我们暂时复用 data 层的结构体定义
// 理想情况下应该定义 UI 专用的 Struct，但为了第一阶段快速重构，先复用
#include "../data/db_storage.h" 

/** @brief 运行由 Application/TaskManager 持有的时间与磁盘监控任务。 */
void uiRunMonitorTask(
    const std::atomic<bool>& stopRequested,
    smart_attendance::app::UiSystemStatusMailbox& statusMailbox);

/** @brief 唤醒正在等待下一次监控周期的任务，以便及时退出。 */
void uiWakeMonitorTask();

/** @brief 注入组合根持有的系统时钟和模拟/真实存储目录，不转移所有权。 */
void uiConfigureDeviceServices(
    smart_attendance::hal::IRtc& rtc,
    smart_attendance::hal::IStorageDevice& storage) noexcept;
void uiResetDeviceServices() noexcept;

/** @brief 将业务层最新帧缩放后投递到 UI 管理器的线程安全缓冲区。 */
void uiRunFrameDeliveryTask(const std::atomic<bool>& stopRequested);

struct UserDisplayInfo {
    int id{0};
    std::string name;
    int departmentId{0};
    std::string departmentName;
    bool faceRegistered{false};
    bool fingerprintRegistered{false};
    std::string cardId;
    bool passwordRegistered{false};
    int role{0};
};

class UiController {
public:
    UiController() = default;
    ~UiController() = default;

    // --- 1. 系统状态类 ---
    bool isDiskFull();             // 替换原来的 check_disk_low
    std::time_t getCurrentUnixTime(); // 读取组合根注入的RTC/系统时钟
    std::string getCurrentTimeStr(); // 替换原来的 get_current_time_str
    std::string getCurrentWeekdayStr();// 获取当前星期几字符串 (例如 "周一")

    // --- 2. 员工管理类 ---
    int generateNextUserId();      // 替换原来的 get_next_available_id
    std::vector<DeptInfo> getDepartmentList();
    std::string getDeptNameById(int deptId);//通过部门ID获取部门名称
    bool registerNewUser(const std::string& name, int deptId);
    int getUserRoleById(int userId);// 获取指定用户的权限 (0:普通, 1:管理员, -1:未找到)

    /**
     * @brief 绑定由 Application 持有的员工角色 Presenter；传 nullptr 清除绑定。
     * @note 不取得所有权，只能在 UI 主线程配置，且必须在服务销毁前清除。
     */
    void configureEmployeeLookupPresenter(
        smart_attendance::ui::EmployeeLookupPresenter* presenter) noexcept;

    void configureSettingsPresenter(
        smart_attendance::ui::SettingsPresenter* presenter) noexcept;

    void configureDepartmentPresenter(
        smart_attendance::ui::DepartmentPresenter* presenter) noexcept;

    void configureShiftPresenter(
        smart_attendance::ui::ShiftPresenter* presenter) noexcept;
    void configureAttendanceQueryPresenter(
        smart_attendance::ui::AttendanceQueryPresenter* presenter) noexcept;
    void configureMaintenancePresenter(
        smart_attendance::ui::MaintenancePresenter* presenter) noexcept;
    void configureSystemInfoPresenter(
        smart_attendance::ui::SystemInfoPresenter* presenter) noexcept;

    // 验证用户密码是否正确(校验哈希值)
    bool verifyUserPassword(int userId, const std::string& inputPassword);
    
    // 获取用于列表显示的用户数据
    // 返回简单的结构或直接复用底层，这里演示获取所有用户
    std::vector<UserData> getAllUsers(); 
    /** @brief 获取详情页字段，不返回密码或生物特征内容。 */
    UserDisplayInfo getUserDisplayInfo(int userId);
    int getUserCount();
    bool getUserAt(int index, int* id, char* name_buf, int buf_len);

    // --- 3. 记录与查询类 ---
    UserData getUserInfo(int uid);
    std::vector<AttendanceRecord> getRecords(int userId, time_t start, time_t end);
    smart_attendance::ui::AttendanceRecordPage getRecordPage(
        int userId, time_t start, time_t end, std::size_t pageIndex);
    // 检查用户是否存在 (用于 UI 导出报表前的同步校验)
    bool checkUserExists(int user_id);
    
    // --- 4. 维护与报表 ---
    bool clearAllRecords();
    bool clearAllEmployees();
    bool factoryReset();
    bool clearAllData();

    // --- 5. 摄像头图像获取  ---
    bool getDisplayFrame(uint8_t* buffer, int width, int height);

    // 更新用户名称
    bool updateUserName(int userId, const std::string& newName);
    //更新用户部门
    bool updateUserDept(int userId, int newDeptId);
    //更新用户人脸
    bool updateUserFace(int userId);
    // 更新用户密码
    bool updateUserPassword(int userId, const std::string& newPassword);
    // 更新用户角色 (0:普通, 1:管理员)
    bool updateUserRole(int userId, int newRole);
    // 删除用户
    bool deleteUser(int userId);

    // 更新摄像头 Buffer 的接口
    void updateCameraFrame(const uint8_t* data, int w, int h);

    //查询系统信息
    SystemStats getSystemStatistics();

     // --- 6. 公司设置类  ---
    bool saveCompanyName(const std::string& name);     // 保存公司名称
    bool loadCompanyName(std::string& name);           // 加载公司名称

    // --- 7. 部门管理类  ---
    bool addDepartment(const std::string& deptName);
    bool updateDepartment(int deptId, const std::string& newName);
    bool deleteDepartment(int deptId);
    int getDepartmentEmployeeCount(int deptId);
    DeptScheduleView getDeptSchedule(int deptId);// 获取指定部门的排班视图
    // 更新指定部门的名称和排班信息
    bool updateDeptSchedule(int deptId, const std::string& newName, const std::vector<int>& shifts);

    
    // --- 8. 班次管理类  ---
    std::vector<ShiftInfo> getAllShifts();//获取所有班次信息

    // 获取指定班次的详细信息，返回 optional 以处理不存在的情况
    std::optional<ShiftInfo> getShiftInfo(int shiftId);
    bool updateShiftInfo(int shift_id, 
                         const std::string& s1_start, const std::string& s1_end,
                         const std::string& s2_start, const std::string& s2_end,
                         const std::string& s3_start, const std::string& s3_end);
    
private:
    // 线程安全相关的成员
    std::mutex m_frame_mutex;            // 保护图像数据的锁
    std::vector<uint8_t> m_cached_frame; // 缓存最新的一帧图像
    std::string m_company_name;          // 公司名称缓存
    std::mutex m_company_mutex;          // 保护公司数据的锁
    smart_attendance::ui::EmployeeLookupPresenter* employeeLookupPresenter_{nullptr};
    smart_attendance::ui::SettingsPresenter* settingsPresenter_{nullptr};
    smart_attendance::ui::DepartmentPresenter* departmentPresenter_{nullptr};
    smart_attendance::ui::ShiftPresenter* shiftPresenter_{nullptr};
    smart_attendance::ui::AttendanceQueryPresenter* attendanceQueryPresenter_{nullptr};
    smart_attendance::ui::MaintenancePresenter* maintenancePresenter_{nullptr};
    smart_attendance::ui::SystemInfoPresenter* systemInfoPresenter_{nullptr};
};

/** @brief 兼容现有员工设置 XLSX 格式的导入实现，由 ReportService 注入调用。 */
bool uiImportEmployeeSettings(
    smart_attendance::storage::IEmployeeSettingsImportRepository& repository,
    int* invalid_time_count = nullptr);

#endif // UI_CONTROLLER_H

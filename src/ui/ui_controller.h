/**
 * @file ui_controller.h
 * @brief UI 控制器头文件 - 提供 UI 层与业务/数据层的接口封装
 * @details 该类封装了 UI 层所需的各种业务逻辑调用，简化 UI 代码复杂度。
 *          通过单例模式提供全局访问点。
 */
#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <string>
#include <vector>
#include <ctime>
#include <mutex>
#include <thread>
#include <atomic>

// 这里为了简化，我们暂时复用 data 层的结构体定义
// 理想情况下应该定义 UI 专用的 Struct，但为了第一阶段快速重构，先复用
#include "../data/db_storage.h" 

class UiController {
public:
    // 单例访问点
    static UiController* getInstance();

    // --- 1. 系统状态类 ---
    bool isDiskFull();             // 替换原来的 check_disk_low
    std::string getCurrentTimeStr(); // 替换原来的 get_current_time_str
    std::string getCurrentWeekdayStr();// 获取当前星期几字符串 (例如 "周一")

    // --- 2. 员工管理类 ---
    int generateNextUserId();      // 替换原来的 get_next_available_id
    std::vector<DeptInfo> getDepartmentList();
    std::string getDeptNameById(int deptId);//通过部门ID获取部门名称
    bool registerNewUser(const std::string& name, int deptId);
    int getUserRoleById(int userId);// 获取指定用户的权限 (0:普通, 1:管理员, -1:未找到)

    // 验证用户密码是否正确(校验哈希值)
    bool verifyUserPassword(int userId, const std::string& inputPassword);
    
    // 获取用于列表显示的用户数据
    // 返回简单的结构或直接复用底层，这里演示获取所有用户
    std::vector<UserData> getAllUsers(); 
    int getUserCount();
    bool getUserAt(int index, int* id, char* name_buf, int buf_len);

    // --- 3. 记录与查询类 ---
    UserData getUserInfo(int uid);
    std::vector<AttendanceRecord> getRecords(int userId, time_t start, time_t end);
    // 检查用户是否存在 (用于 UI 导出报表前的同步校验)
    bool checkUserExists(int user_id);
    
    // --- 4. 维护与报表 ---
    bool exportReportToUsb();      // 封装报表导出逻辑
    void clearAllRecords();
    void clearAllEmployees();
    void factoryReset();
    void clearAllData();
    bool exportEmployeeSettings();// 导出员工设置表
    // 上传员工设置表（从U盘读取xlsx并导入数据库）
    // invalid_time_count: 若非空，写入解析过程中时间格式非法的字段数
    bool importEmployeeSettings(int* invalid_time_count = nullptr);

    // --- 5. 摄像头图像获取  ---
    bool getDisplayFrame(uint8_t* buffer, int width, int height);

    void startBackgroundServices(); // 启动所有后台线程
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

    // 导出自定义报表
    bool exportCustomReport(const std::string& start, const std::string& end);
    
    // 导出个人报表
    bool exportUserReport(int user_id, const std::string& start, const std::string& end);
    
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
    
private:
    UiController() = default; // 私有构造
    ~UiController() = default;

    void monitorThreadFunc(); // 监控时间与磁盘
    void captureThreadFunc(); // 监控摄像头 (从 ui_app.cpp 移入)
    
    // 线程控制
    std::atomic<bool> m_running{false};
    std::thread m_monitor_thread;
    std::thread m_capture_thread;

    // 线程安全相关的成员
    std::mutex m_frame_mutex;            // 保护图像数据的锁
    std::vector<uint8_t> m_cached_frame; // 缓存最新的一帧图像
    std::string m_company_name;          // 公司名称缓存
    std::mutex m_company_mutex;          // 保护公司数据的锁
};

#endif // UI_CONTROLLER_H
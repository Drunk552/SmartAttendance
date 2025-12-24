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

    // --- 2. 员工管理类 ---
    int generateNextUserId();      // 替换原来的 get_next_available_id
    std::vector<DeptInfo> getDepartmentList();
    bool registerNewUser(const std::string& name, int deptId);
    
    // 获取用于列表显示的用户数据
    // 返回简单的结构或直接复用底层，这里演示获取所有用户
    std::vector<UserData> getAllUsers(); 
    int getUserCount();
    bool getUserAt(int index, int* id, char* name_buf, int buf_len);

    // --- 3. 记录与查询类 ---
    UserData getUserInfo(int uid);
    std::vector<AttendanceRecord> getRecords(int userId, time_t start, time_t end);
    
    // --- 4. 维护与报表 ---
    bool exportReportToUsb();      // 封装报表导出逻辑
    void clearAllRecords();
    void clearAllEmployees();
    void factoryReset();
    void clearAllData();

    // --- 5. 摄像头图像获取  ---
    bool getDisplayFrame(uint8_t* buffer, int width, int height);

private:
    UiController() = default; // 私有构造
    ~UiController() = default;
};

#endif // UI_CONTROLLER_H
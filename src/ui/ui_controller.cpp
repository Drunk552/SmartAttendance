/**
 * @file ui_controller.cpp
 * @brief UI 控制器实现文件 - 提供 UI 层与业务/数据层的接口封装
 * @details 该类封装了 UI 层所需的各种业务逻辑调用，简化 UI 代码复杂度。
 *          通过单例模式提供全局访问点。
 */
#include "ui_controller.h"
// 引入原来 ui_app.cpp 依赖的底层头文件
#include "../data/db_storage.h"
#include "../business/face_demo.h"
#include "../business/report_generator.h"
#include "../business/event_bus.h"
#include <sys/statvfs.h>
#include <algorithm>
#include <set>
#include <cstdio>
#include <filesystem>
#include <thread> // for sleep if needed

static UiController* s_instance = nullptr;

UiController* UiController::getInstance() {
    if (!s_instance) {
        s_instance = new UiController();
    }
    return s_instance;
}

// 移入原 check_disk_low 逻辑
bool UiController::isDiskFull() {
    struct statvfs stat;
    if (statvfs(".", &stat) != 0) return false;
    unsigned long long free_bytes = stat.f_bavail * stat.f_frsize;
    unsigned long long free_mb = free_bytes / (1024 * 1024);
    return (free_mb < 100);
}

// 移入原 get_current_time_str 逻辑
std::string UiController::getCurrentTimeStr() {
    std::time_t rawtime = std::time(nullptr);
    std::tm *timeinfo = std::localtime(&rawtime);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", timeinfo);
    return std::string(buf);
}

// 移入原 get_next_available_id 逻辑
int UiController::generateNextUserId() {
    std::vector<UserData> users = db_get_all_users();
    std::set<int> existing_ids;
    for (const auto& user : users) {
        existing_ids.insert(user.id);
    }
    int next_id = 1;
    while (existing_ids.find(next_id) != existing_ids.end()) {
        next_id++;
    }
    return next_id;
}

std::vector<DeptInfo> UiController::getDepartmentList() {
    return db_get_departments();
}

bool UiController::registerNewUser(const std::string& name, int deptId) {
    // 调用业务层接口
    return business_register_user(name.c_str(), deptId);
}

std::vector<UserData> UiController::getAllUsers() {
    return db_get_all_users();
}

int UiController::getUserCount() {
    return business_get_user_count();
}

bool UiController::getUserAt(int index, int* id, char* name_buf, int buf_len) {
    return business_get_user_at(index, id, name_buf, buf_len);
}

UserData UiController::getUserInfo(int uid) {
    return db_get_user_info(uid);
}

std::vector<AttendanceRecord> UiController::getRecords(int userId, time_t start, time_t end) {
    // 可以在这里对数据进行过滤，UI层拿到的就是处理好的
    auto all_records = db_get_records(start, end);
    if (userId < 0) return all_records; // 约定 < 0 返回所有

    std::vector<AttendanceRecord> filtered;
    for (const auto& rec : all_records) {
        if (rec.user_id == userId) filtered.push_back(rec);
    }
    return filtered;
}

bool UiController::exportReportToUsb() {
    // 将原 ui_download_report_handler 中的逻辑移到这里
    // 包括创建目录、计算日期、调用 Generator
    // 这里只保留纯业务，不包含 lv_msgbox 等 UI 弹窗代码
    // 可以返回一个状态码或 bool 给 UI 层去决定弹窗内容
    try {
        std::filesystem::create_directories("output/usb_sim");
    } catch (...) { return false; }

    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char start_date[16], end_date[16];
    std::strftime(start_date, sizeof(start_date), "%Y-%m-01", now);
    std::strftime(end_date, sizeof(end_date), "%Y-%m-%d", now);

    ReportGenerator generator;

    return generator.exportReport(ReportType::SUMMARY, 
                                  start_date, end_date, 
                                  "output/usb_sim/attendance_report.xlsx");
}

bool UiController::getDisplayFrame(uint8_t* buffer, int width, int height) {
    // 1. 加锁
    std::lock_guard<std::mutex> lock(m_frame_mutex);
    
    // 2. 检查是否有数据
    if (m_cached_frame.empty()) {
        return false; // 还没采集到第一帧
    }
    
    // 3. 检查缓冲区大小是否匹配 (防止越界)
    size_t required_size = width * height * 3;
    if (m_cached_frame.size() < required_size) {
        return false;
    }

    // 4. [快速操作] 仅拷贝内存，不进行任何硬件IO
    std::memcpy(buffer, m_cached_frame.data(), required_size);
    
    return true;
}

// 更新用户名称实现
bool UiController::updateUserName(int userId, const std::string& newName) {
    // 1. 先获取当前用户完整信息
    UserData user = db_get_user_info(userId);
    
    // 2. 检查用户是否存在 (假设ID为0表示不存在)
    if (user.id == 0) return false; 

    // 3. 调用底层更新接口：
    //    保留原有的 dept_id, role, card_id 不变，只修改 name
    return db_update_user_basic(userId, newName, user.dept_id, user.role, user.card_id);
}

// 更新用户密码实现
bool UiController::updateUserPassword(int userId, const std::string& newPassword) {
    // 底层有单独修改密码的接口，直接调用即可
    return db_update_user_password(userId, newPassword);
}

// 更新用户角色实现
bool UiController::updateUserRole(int userId, int newRole) {
    // 1. 获取当前信息
    UserData user = db_get_user_info(userId);
    if (user.id == 0) return false;

    // 2. 调用底层更新接口：
    //    保留原有的 name, dept_id, card_id 不变，只修改 role (privilege)
    return db_update_user_basic(userId, user.name, user.dept_id, newRole, user.card_id);
}

// 删除用户实现
bool UiController::deleteUser(int userId) {
    // 调用底层数据库接口
    // 注意：db_delete_user 会级联删除该用户的考勤记录和图片，非常干净
    return db_delete_user(userId);
}

void UiController::clearAllRecords() {
    db_clear_attendance();
}

// 恢复出厂设置实现
void UiController::factoryReset() {
    // 调用底层数据层的重置接口
    db_factory_reset();
}

// 清除所有员工实现 (防止下一个报错是它)
void UiController::clearAllEmployees() {
    db_clear_users();
}

// 清除所有数据实现
void UiController::clearAllData() {
    // 假设底层有这个函数，或者手动调用清除员工+清除记录
    db_clear_users();
    db_clear_attendance();
    // 可能还需要删除特征文件等，视具体业务而定
}

// 后台服务与事件总线
void UiController::startBackgroundServices() {
    if (m_running) return;
    m_running = true;

    // 1. 启动系统监控线程
    m_monitor_thread = std::thread(&UiController::monitorThreadFunc, this);
    m_monitor_thread.detach();

    // 2. 启动摄像头采集线程
    m_capture_thread = std::thread(&UiController::captureThreadFunc, this);
    m_capture_thread.detach();
}

// 监控线程实现
void UiController::monitorThreadFunc() {
    while (m_running) {
        // A. 时间事件 (每秒)
        std::string timeStr = getCurrentTimeStr();
        EventBus::getInstance().publish(EventType::TIME_UPDATE, &timeStr);

        // B. 磁盘监控 (每 5 秒)
        static int disk_check_counter = 0;
        if (++disk_check_counter >= 5) {
            disk_check_counter = 0;
            if (isDiskFull()) EventBus::getInstance().publish(EventType::DISK_FULL);
            else EventBus::getInstance().publish(EventType::DISK_NORMAL);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// 摄像头采集线程
void UiController::captureThreadFunc() {
    // 临时读取缓冲 (避免长时间持有锁)
    std::vector<uint8_t> temp_buf(240 * 180 * 3); 

    while (m_running) {
        // 1. [耗时操作] 从底层业务/摄像头获取数据
        // 注意：这是唯一调用 business_get_display_frame 的地方！
        bool ret = business_get_display_frame(temp_buf.data(), 240, 180);
        
        if (ret) {
            // 2. [快速操作] 加锁，将数据更新到 cached_frame
            {
                std::lock_guard<std::mutex> lock(m_frame_mutex);
                // 如果缓存区大小不对，调整大小
                if (m_cached_frame.size() != temp_buf.size()) {
                    m_cached_frame.resize(temp_buf.size());
                }
                // 内存拷贝
                std::memcpy(m_cached_frame.data(), temp_buf.data(), temp_buf.size());
            } // 锁在这里自动释放

            // 3. 通知 UI 线程“数据准备好了”
            EventBus::getInstance().publish(EventType::CAMERA_FRAME_READY, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(33));// 大约30FPS
        } else {
            // 获取失败 (比如摄像头没插好)，休眠长一点避免空转
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
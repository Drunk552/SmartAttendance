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
    // 调用底层的 business 接口
    return business_get_display_frame(buffer, width, height);
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
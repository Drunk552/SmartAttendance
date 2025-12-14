/**
 * @file db_storage.cpp
 * @brief 数据层实现 - Phase 02 Schema Evolution
 */

#include "db_storage.h"
#include <sqlite3.h>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <filesystem> // C++17 标准库，用于处理文件系统
#include <sys/stat.h>

namespace fs = std::filesystem;

// 配置：图片存储的文件夹名称
const std::string IMAGE_DIR = "captured_images";
// 配置：数据库文件名
const std::string DB_NAME = "attendance.db";

// 数据库连接句柄 (静态全局变量，仅本文件可见)
static sqlite3* db = nullptr;

// 辅助函数：将 Mat 序列化为 vector<uchar> (用于存 BLOB)
std::vector<uchar> matToBytes(const cv::Mat& image) {
    std::vector<uchar> buf;
    // 将图片编码为 jpg 格式的内存流
    cv::imencode(".jpg", image, buf); 
    return buf;
}

// 辅助函数：将 vector<uchar> 反序列化为 Mat (用于读 BLOB)
cv::Mat bytesToMat(const std::vector<uchar>& bytes) {
    return cv::imdecode(bytes, cv::IMREAD_GRAYSCALE); // 假设训练用灰度图
}

// 执行简单SQL的辅助函数
static bool exec_sql(const char* sql, const char* error_prefix) {
    char* zErrMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] SQL Error (" << error_prefix << "): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

bool data_init() {
    // 1. 初始化图片存储目录
    try {
        if (!fs::exists(IMAGE_DIR)) {
            if (fs::create_directories(IMAGE_DIR)) {
                std::cout << "[Data] Created image directory: " << IMAGE_DIR << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Data] Filesystem error: " << e.what() << std::endl;
        return false;
    }

    // 2. 打开/创建数据库
    int rc = sqlite3_open(DB_NAME.c_str(), &db);
    if (rc) {
        std::cerr << "[Data] Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 3. 开启外键支持 (SQLite 默认关闭)
    exec_sql("PRAGMA foreign_keys = ON;", "Enable Foreign Keys");

    // ==========================================
    // Epic 2.1: 数据库架构重构 (建表)
    // ==========================================

    // (A) 部门表 (departments)
    const char* sql_dept = 
        "CREATE TABLE IF NOT EXISTS departments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL UNIQUE);";
    if (!exec_sql(sql_dept, "Create Departments")) return false;

    // (B) 班次表 (shifts)
    const char* sql_shifts = 
        "CREATE TABLE IF NOT EXISTS shifts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "start_time TEXT, " // HH:MM
        "end_time TEXT, "   // HH:MM
        "cross_day INTEGER DEFAULT 0);"; // 0 or 1
    if (!exec_sql(sql_shifts, "Create Shifts")) return false;

    // (C) 考勤规则/系统配置表 (attendance_rules)
    const char* sql_rules = 
        "CREATE TABLE IF NOT EXISTS attendance_rules ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "company_name TEXT, "
        "late_threshold INTEGER DEFAULT 0, "       // 迟到阈值(分钟)
        "early_leave_threshold INTEGER DEFAULT 0);"; // 早退阈值(分钟)
    if (!exec_sql(sql_rules, "Create Rules")) return false;

    // (D) 用户表 (users) - 核心升级
    // 包含 密码、卡号、权限、部门关联
    const char* sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "password TEXT, "
        "card_id TEXT, "
        "privilege INTEGER DEFAULT 0, " // 0:User, 1:Admin
        "face_data BLOB, "
        "dept_id INTEGER, "
        "FOREIGN KEY(dept_id) REFERENCES departments(id) ON DELETE SET NULL);";
    if (!exec_sql(sql_users, "Create Users")) return false;

    // (E) 考勤记录表 (attendance) - 关联增强
    // 关联 用户、班次，增加状态字段
    const char* sql_attendance = 
        "CREATE TABLE IF NOT EXISTS attendance ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "user_id INTEGER NOT NULL, "
        "shift_id INTEGER, "
        "image_path TEXT, "
        "timestamp INTEGER, "
        "status INTEGER DEFAULT 0, " // 0:Normal, 1:Late, etc.
        "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
        "FOREIGN KEY(shift_id) REFERENCES shifts(id) ON DELETE SET NULL);";
    if (!exec_sql(sql_attendance, "Create Attendance")) return false;

    std::cout << "[Data] Phase 02 Database Schema Initialized Successfully." << std::endl;
    return true;
}

// [新增实现] 注册用户 - 二进制存储
// ---------------------------------------------------------
// 兼容性适配：为了让 Phase 1 的代码能跑，我们需要适配新表结构
// ---------------------------------------------------------
int data_registerUser(const std::string& name, const cv::Mat& face_image) {
    if (face_image.empty()) return -1;

    // 1. 将图像转换为二进制流
    std::vector<uchar> blob_data = matToBytes(face_image);

    // 暂时只插入 name 和 face_data，其他字段允许为空或默认
    const char* sql = "INSERT INTO users (name, face_data, privilege) VALUES (?, ?, 0);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return -1;

    // 2. 绑定参数
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    // 关键：绑定 BLOB 数据
    sqlite3_bind_blob(stmt, 2, blob_data.data(), blob_data.size(), SQLITE_TRANSIENT);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result = (int)sqlite3_last_insert_rowid(db);
        std::cout << "[Data] User registered (Legacy Compat): " << name << " ID: " << result << std::endl;
    }
    sqlite3_finalize(stmt);
    return result;
}

// [新增实现] 保存考勤 - 路径存储
bool data_saveAttendance(int user_id, const cv::Mat& image) {
    if (image.empty()) return false;

    // 1. 生成文件并保存到磁盘
    std::time_t now = std::time(nullptr);
    std::string filename = std::to_string(now) + "_" + std::to_string(user_id) + ".jpg";
    fs::path full_path = fs::path(IMAGE_DIR) / filename;
    
    try {
        if (!cv::imwrite(full_path.string(), image)) return false;
    } catch (...) { return false; }

    // 2. 将路径插入数据库
    const char* sql = "INSERT INTO attendance (user_id, image_path, timestamp, status) VALUES (?, ?, ?, 0);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, full_path.string().c_str(), -1, SQLITE_STATIC); // 存路径
    sqlite3_bind_int64(stmt, 3, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if(success) std::cout << "[Data] Attendance saved: " << user_id << std::endl;
    return success;
}

// [新增实现] 读取所有用户（用于初始化训练）
std::vector<UserData> data_getAllUsers() {
    std::vector<UserData> users;
    // 更新 SQL 以匹配新架构
    const char* sql = "SELECT id, name, face_data, privilege, dept_id FROM users;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserData u;
            u.id = sqlite3_column_int(stmt, 0);
            u.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            // 读取 BLOB
            const void* blob = sqlite3_column_blob(stmt, 2);
            int bytes = sqlite3_column_bytes(stmt, 2);
            u.face_feature.assign((const uchar*)blob, (const uchar*)blob + bytes);
            
            // 读取新字段，处理可能的 NULL
            u.role = sqlite3_column_int(stmt, 3);
            u.dept_id = sqlite3_column_int(stmt, 4);

            users.push_back(u);
        }
    }
    sqlite3_finalize(stmt);
    return users;
}

void data_close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "[Data] Database connection closed." << std::endl;
    }
}

/**
 * @file db_storage.cpp
 * @brief 数据层实现 - 文件系统存储 + 数据库索引模式
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

    // 3. [关键修改] 创建表结构：混合存储模式
    char* zErrMsg = 0;

    // (A) 创建用户表：使用 BLOB 存储人脸特征二进制数据
    const char* sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "face_data BLOB);";  // <--- 核心点：这里是 BLOB 类型

    rc = sqlite3_exec(db, sql_users, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] SQL error (create users): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }

    // (B) 创建考勤表：使用 TEXT 存储图片路径，并关联用户ID
    // 注意：我们将原本的 processed_images 表的功能升级为 attendance 表
    const char* sql_attendance = 
        "CREATE TABLE IF NOT EXISTS attendance ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "user_id INTEGER, "     // 关联到 users 表的 id
        "image_path TEXT, "     // <--- 核心点：这里存文件路径，不存二进制
        "timestamp INTEGER, "
        "FOREIGN KEY(user_id) REFERENCES users(id));";

    rc = sqlite3_exec(db, sql_attendance, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] SQL error (create attendance): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }
    
    std::cout << "[Data] Database initialized successfully (Mixed Storage Mode)." << std::endl;
    return true;
}

// [新增实现] 注册用户 - 二进制存储
int data_registerUser(const std::string& name, const cv::Mat& face_image) {
    if (face_image.empty()) return -1;

    // 1. 将图像转换为二进制流
    std::vector<uchar> blob_data = matToBytes(face_image);

    const char* sql = "INSERT INTO users (name, face_data) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return -1;

    // 2. 绑定参数
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    // 关键：绑定 BLOB 数据
    sqlite3_bind_blob(stmt, 2, blob_data.data(), blob_data.size(), SQLITE_TRANSIENT);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result = (int)sqlite3_last_insert_rowid(db);
        std::cout << "[Data] User registered: " << name << " ID: " << result << " (BLOB Size: " << blob_data.size() << ")" << std::endl;
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
    const char* sql = "INSERT INTO attendance (user_id, image_path, timestamp) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, full_path.string().c_str(), -1, SQLITE_STATIC); // 存路径
    sqlite3_bind_int64(stmt, 3, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if(success) std::cout << "[Data] Attendance saved: " << full_path << std::endl;
    return success;
}

// [新增实现] 读取所有用户（用于初始化训练）
std::vector<UserData> data_getAllUsers() {
    std::vector<UserData> users;
    const char* sql = "SELECT id, name, face_data FROM users;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserData u;
            u.id = sqlite3_column_int(stmt, 0);
            u.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            // 读取 BLOB
            const void* blob_ptr = sqlite3_column_blob(stmt, 2);
            int blob_size = sqlite3_column_bytes(stmt, 2);
            u.face_feature.assign((const uchar*)blob_ptr, (const uchar*)blob_ptr + blob_size);
            
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

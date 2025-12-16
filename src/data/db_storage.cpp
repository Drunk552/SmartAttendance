/**
 * @file db_storage.cpp
 * @brief 数据层实现 - Phase 02 Epic 2.2 DAO Impl
 * @details 实现了数据库初始化、表结构升级以及部门、班次、用户、考勤的 CRUD 操作。
 * @version 2.0
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

// ================= 辅助函数 (Helpers)将 Mat 序列化为 vector<uchar> (用于存 BLOB) =================

/**
 * @brief 图片转二进制流 (OpenCV Mat -> vector<uchar>)
 */
static std::vector<uchar> matToBytes(const cv::Mat& image) {
    std::vector<uchar> buf;
    if (!image.empty()) {
        // 编码为 .jpg 格式以节省空间，同时保留特征
        cv::imencode(".jpg", image, buf); 
    }
    return buf;
}

/**
 * @brief 二进制流转图片 (vector<uchar> -> OpenCV Mat)
 * @details 用于从数据库 BLOB 读取数据后还原为图像（主要用于内部校验或调试）
 */
static cv::Mat bytesToMat(const std::vector<uchar>& bytes) {
    if (bytes.empty()) return cv::Mat();
    // 解码内存中的 JPG 数据为灰度图 (保持与 Phase 1 一致，利于 LBPH 识别)
    return cv::imdecode(bytes, cv::IMREAD_GRAYSCALE);
}

/**
 * @brief 执行无返回值的 SQL 语句
 * @param sql SQL 命令字符串
 * @param tag用于日志报错的标签
 */
static bool exec_sql(const char* sql, const char* tag) {
    char* zErrMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK) {
        std::cerr << "[Data] SQL Error (" << tag << "): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

// ================= 核心生命周期 (Lifecycle) =================

bool data_init() {
    // 1. 确保存储目录存在
    try {
        if (!fs::exists(IMAGE_DIR)) fs::create_directories(IMAGE_DIR);
    } catch (const std::exception& e) {
        std::cerr << "[Data] FS Init Error: " << e.what() << std::endl;
        return false;
    }

    // 2. 连接数据库
    if (sqlite3_open(DB_NAME.c_str(), &db)) {
        std::cerr << "[Data] Can't open DB: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 3. 开启外键约束 (SQLite 默认关闭)
    exec_sql("PRAGMA foreign_keys = ON;", "Enable FK");

    // 4. 创建/更新表结构 (Phase 02 Schema)
    
    // (A) 部门表
    const char* sql_dept = 
        "CREATE TABLE IF NOT EXISTS departments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL UNIQUE);";

    // (B) 班次表
    const char* sql_shifts = 
        "CREATE TABLE IF NOT EXISTS shifts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT, "
        "start_time TEXT, "
        "end_time TEXT, "
        "cross_day INTEGER DEFAULT 0);";

    // (C) 考勤规则表
    const char* sql_rules = 
        "CREATE TABLE IF NOT EXISTS attendance_rules ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "company_name TEXT, "
        "late_threshold INTEGER DEFAULT 0, "
        "early_leave_threshold INTEGER DEFAULT 0);";
    
    // (D) 用户表 (核心升级：包含权限、密码、关联部门、人脸BLOB)
    const char* sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "password TEXT, "
        "card_id TEXT, "
        "privilege INTEGER DEFAULT 0, " // 0:User, 1:Admin
        "face_data BLOB, "              // 人脸二进制数据
        "dept_id INTEGER, "
        "FOREIGN KEY(dept_id) REFERENCES departments(id) ON DELETE SET NULL);";

    // (E) 考勤记录表 (关联用户与班次)
    const char* sql_att = 
        "CREATE TABLE IF NOT EXISTS attendance ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "user_id INTEGER NOT NULL, "
        "shift_id INTEGER, "
        "image_path TEXT, "
        "timestamp INTEGER, "
        "status INTEGER DEFAULT 0, "
        "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
        "FOREIGN KEY(shift_id) REFERENCES shifts(id) ON DELETE SET NULL);";

    bool ret = exec_sql(sql_dept, "Create Dept") && 
               exec_sql(sql_shifts, "Create Shifts") &&
               exec_sql(sql_rules, "Create Rules") && 
               exec_sql(sql_users, "Create Users") &&
               exec_sql(sql_att, "Create Attendance");
    
    if(ret) std::cout << "[Data] DAO Layer Initialized (Phase 2)." << std::endl;
    
    if (ret) {
        std::cout << "[Data] DAO Layer Initialized (Phase 2)." << std::endl;
        // 执行播种
        data_seed();
    }
    
    return ret;
}

// [新增辅助函数] 检查表中是否有数据
static bool is_table_empty(const char* table_name) {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT COUNT(*) FROM " + std::string(table_name) + ";";
    int count = 0;
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return (count == 0);
}

// ================= Epic 2.3 数据播种 =================

bool data_seed() {
    std::cout << ">>> [Data] Checking for data seeding..." << std::endl;

    // 1. 播种默认部门 (如果为空)
    if (is_table_empty("departments")) {
        // 对应手册中的 "Not Set" [cite: 1242]
        if (db_add_department("Not Set")) {
            std::cout << "   [Seed] Created default department: 'Not Set'" << std::endl;
        }
        // 可选：添加常用部门
        db_add_department("R&D");
        db_add_department("HR");
    }

    // 2. 播种默认班次 (如果为空)
    if (is_table_empty("shifts")) {
        // 对应任务书要求的默认班次 [cite: 2003]
        const char* sql = "INSERT INTO shifts (name, start_time, end_time, cross_day) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            // 班次一: 08:00 - 18:00 (简化版，或者按任务书分段但这里只有start/end)
            sqlite3_bind_text(stmt, 1, "General Shift", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, "09:00", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, "18:00", -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 4, 0); // 不跨天
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            std::cout << "   [Seed] Created default shift: 'General Shift' (09:00-18:00)" << std::endl;
        }
    }

    // 3. 播种默认管理员 (如果用户表为空)
    if (is_table_empty("users")) {
        UserData admin;
        admin.name = "SuperAdmin";
        admin.password = "888888"; // 默认密码
        admin.card_id = "000000";
        admin.role = 1; // 管理员
        
        // 获取刚才播种的部门ID
        auto depts = db_get_departments();
        admin.dept_id = depts.empty() ? 0 : depts[0].id; // 归属到第一个部门

        // 创建一个空的黑色人脸图作为占位符
        cv::Mat dummy_face = cv::Mat::zeros(64, 64, CV_8UC1);
        
        int uid = db_add_user(admin, dummy_face);
        if (uid > 0) {
            std::cout << "   [Seed] Created default admin: 'SuperAdmin' (ID: " << uid << ", Pwd: 888888)" << std::endl;
        }
    }

    return true;
}

void data_close() {
    if (db) { 
        sqlite3_close(db); 
        db = nullptr; 
        std::cout << "[Data] Database connection closed." << std::endl;
    }
}

// ================= 1. 部门管理 DAO =================

bool db_add_department(const std::string& dept_name) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO departments (name) VALUES (?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, dept_name.c_str(), -1, SQLITE_STATIC);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<DeptInfo> db_get_departments() {
    std::vector<DeptInfo> list;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name FROM departments;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DeptInfo d;
            d.id = sqlite3_column_int(stmt, 0);
            const char* txt = (const char*)sqlite3_column_text(stmt, 1);
            d.name = txt ? txt : "";
            list.push_back(d);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

bool db_delete_department(int dept_id) {
    const char* sql = "DELETE FROM departments WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt, 1, dept_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ================= 2. 班次管理 DAO =================

bool db_update_shift(int shift_id, const std::string& start, const std::string& end, int cross_day) {
    // 简化处理：直接更新。实际业务中可能需要先判断ID是否存在
    const char* sql = "UPDATE shifts SET start_time=?, end_time=?, cross_day=? WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, cross_day);
    sqlite3_bind_int(stmt, 4, shift_id);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<ShiftInfo> db_get_shifts() {
    std::vector<ShiftInfo> list;
    sqlite3_stmt* stmt;
    // 假设系统规划最多支持16个班次
    const char* sql = "SELECT id, name, start_time, end_time, cross_day FROM shifts;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ShiftInfo s;
            s.id = sqlite3_column_int(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            const char* start = (const char*)sqlite3_column_text(stmt, 2);
            const char* end = (const char*)sqlite3_column_text(stmt, 3);
            
            s.name = name ? name : "";
            s.start_time = start ? start : "--:--";
            s.end_time = end ? end : "--:--";
            s.cross_day = sqlite3_column_int(stmt, 4);
            
            list.push_back(s);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

// ================= 3. 用户管理 DAO (升级版) =================

int db_add_user(const UserData& info, const cv::Mat& face_img) {
    // 1. 转换人脸图片为 BLOB
    std::vector<uchar> blob = matToBytes(face_img);
    if (blob.empty()) {
        std::cerr << "[Data] Error: Face image is empty!" << std::endl;
        return -1;
    }

    const char* sql = 
        "INSERT INTO users (name, password, card_id, privilege, dept_id, face_data) "
        "VALUES (?, ?, ?, ?, ?, ?);";
        
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return -1;

    // 2. 绑定参数
    sqlite3_bind_text(stmt, 1, info.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, info.password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, info.card_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, info.role);
    
    if (info.dept_id > 0) 
        sqlite3_bind_int(stmt, 5, info.dept_id); 
    else 
        sqlite3_bind_null(stmt, 5); // 无部门时设为 NULL
        
    sqlite3_bind_blob(stmt, 6, blob.data(), (int)blob.size(), SQLITE_TRANSIENT);

    // 3. 执行
    int new_id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        new_id = (int)sqlite3_last_insert_rowid(db);
        std::cout << "[Data] New User Added. ID: " << new_id << ", Name: " << info.name << std::endl;
    } else {
        std::cerr << "[Data] Insert User Failed: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
    return new_id;
}

UserData db_get_user_info(int user_id) {
    UserData u;
    u.id = 0; // 0 表示无效/未找到
    
    // 注意：此接口通常用于 UI 显示详情，暂不读取 face_data 以提升性能
    const char* sql = "SELECT id, name, password, card_id, privilege, dept_id FROM users WHERE id=?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            u.id = sqlite3_column_int(stmt, 0);
            u.name = (const char*)sqlite3_column_text(stmt, 1);
            
            const char* pwd = (const char*)sqlite3_column_text(stmt, 2);
            u.password = pwd ? pwd : "";
            
            const char* card = (const char*)sqlite3_column_text(stmt, 3);
            u.card_id = card ? card : "";
            
            u.role = sqlite3_column_int(stmt, 4);
            u.dept_id = sqlite3_column_int(stmt, 5);
        }
    }
    sqlite3_finalize(stmt);
    return u;
}

bool db_delete_user(int user_id) {
    const char* sql = "DELETE FROM users WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt, 1, user_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<UserData> db_get_all_users() {
    std::vector<UserData> list;
    // 注意：此接口用于模型训练，必须读取 face_data
    const char* sql = "SELECT id, name, face_data, privilege, dept_id FROM users;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserData u;
            u.id = sqlite3_column_int(stmt, 0);
            u.name = (const char*)sqlite3_column_text(stmt, 1);
            
            // 读取 BLOB 到 vector<uchar>
            const void* blob = sqlite3_column_blob(stmt, 2);
            int bytes = sqlite3_column_bytes(stmt, 2);
            if(blob && bytes > 0) {
                u.face_feature.assign((const uchar*)blob, (const uchar*)blob + bytes);
            }
            
            u.role = sqlite3_column_int(stmt, 3);
            u.dept_id = sqlite3_column_int(stmt, 4);
            
            list.push_back(u);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

// ================= 4. 考勤记录 DAO =================

bool db_log_attendance(int user_id, int shift_id, const cv::Mat& image, int status) {
    long long now = std::time(nullptr);
    std::string path_str = "";
    
    // 1. 保存图片到磁盘
    if (!image.empty()) {
        std::string fname = std::to_string(now) + "_" + std::to_string(user_id) + ".jpg";
        fs::path p = fs::path(IMAGE_DIR) / fname;
        try {
            if (cv::imwrite(p.string(), image)) {
                path_str = p.string();
            }
        } catch (...) {
            std::cerr << "[Data] Save Image Failed." << std::endl;
        }
    }

    // 2. 插入数据库
    const char* sql = 
        "INSERT INTO attendance (user_id, shift_id, image_path, timestamp, status) "
        "VALUES (?, ?, ?, ?, ?);";
        
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, user_id);
    
    if(shift_id > 0) 
        sqlite3_bind_int(stmt, 2, shift_id); 
    else 
        sqlite3_bind_null(stmt, 2);
        
    sqlite3_bind_text(stmt, 3, path_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int(stmt, 5, status);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if(ok) {
        std::cout << "[Data] Attendance Logged -> User: " << user_id 
                  << " Time: " << now << " Status: " << status << std::endl;
    }
    return ok;
}

std::vector<AttendanceRecord> db_get_records(long long start_ts, long long end_ts) {
    std::vector<AttendanceRecord> list;
    
    // 关联查询：attendance -> users -> departments
    const char* sql = 
        "SELECT a.id, a.user_id, a.timestamp, a.status, a.image_path, u.name, d.name "
        "FROM attendance a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE a.timestamp BETWEEN ? AND ? "
        "ORDER BY a.timestamp DESC;";
        
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, start_ts);
        sqlite3_bind_int64(stmt, 2, end_ts);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AttendanceRecord r;
            r.id = sqlite3_column_int(stmt, 0);
            r.user_id = sqlite3_column_int(stmt, 1);
            r.timestamp = sqlite3_column_int64(stmt, 2);
            r.status = sqlite3_column_int(stmt, 3);
            
            const char* p = (const char*)sqlite3_column_text(stmt, 4);
            r.image_path = p ? p : "";
            
            const char* uname = (const char*)sqlite3_column_text(stmt, 5);
            r.user_name = uname ? uname : "Unknown";
            
            const char* dname = (const char*)sqlite3_column_text(stmt, 6);
            r.dept_name = dname ? dname : "No Dept";
            
            list.push_back(r);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

// ================= 兼容层 (Phase 1 Support) =================

int data_registerUser(const std::string& name, const cv::Mat& face_image) {
    // 适配旧接口：创建一个默认的 UserData 对象
    UserData info;
    info.name = name;
    info.role = 0;      // 默认为普通用户
    info.dept_id = 0;   // 默认无部门
    info.password = ""; 
    info.card_id = "";

    // 调用新接口
    return db_add_user(info, face_image);
}

bool data_saveAttendance(int user_id, const cv::Mat& image) {
    // 适配旧接口：shift_id=0, status=0 (正常)
    return db_log_attendance(user_id, 0, image, 0);
}

/*
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
*/


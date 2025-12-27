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
#include <functional> // 引入哈希支持
#include <sstream>
#include <iomanip>
#include <mutex>// 用于线程安全

namespace fs = std::filesystem;

// 配置：图片存储的文件夹名称
const std::string IMAGE_DIR = "captured_images";
// 配置：数据库文件名
const std::string DB_NAME = "attendance.db";

static sqlite3* db = nullptr;// 数据库连接句柄 (静态全局变量，仅本文件可见)
static std::recursive_mutex g_db_mutex;// 递归互斥锁，确保线程安全

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
        "s1_start TEXT, s1_end TEXT, "
        "s2_start TEXT, s2_end TEXT, "
        "s3_start TEXT, s3_end TEXT, "
        "cross_day INTEGER DEFAULT 0);";

    // (C) 考勤规则表
    const char* sql_rules = 
        "CREATE TABLE IF NOT EXISTS attendance_rules ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "company_name TEXT, "
        "late_threshold INTEGER DEFAULT 15, "
        "early_leave_threshold INTEGER DEFAULT 0, "
        // 新增字段
        "device_id INTEGER DEFAULT 1, "
        "volume INTEGER DEFAULT 70, "
        "screensaver_time INTEGER DEFAULT 0, " // 0=关闭
        "max_admins INTEGER DEFAULT 10, "
        "relay_delay INTEGER DEFAULT 5, "      // 默认开门5秒
        "wiegand_fmt INTEGER DEFAULT 26 "      // 默认韦根26
        ");";
    
    // (D) 用户表 (核心升级：包含权限、密码、关联部门、人脸BLOB)
    const char* sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "password TEXT, "
        "card_id TEXT, "
        "privilege INTEGER DEFAULT 0, " // 0:User, 1:Admin
        "face_data BLOB, "              // 人脸二进制数据
        "fingerprint_data BLOB, "
        "dept_id INTEGER, "
        "default_shift_id INTEGER, " // 绑定的默认班次ID
        "FOREIGN KEY(dept_id) REFERENCES departments(id) ON DELETE SET NULL, "
        "FOREIGN KEY(default_shift_id) REFERENCES shifts(id) ON DELETE SET NULL);"; // 外键约束

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

    // (F) 部门周排班表 (联合主键确保一个部门一天只有一条规则)
    const char* sql_dept_sch = 
        "CREATE TABLE IF NOT EXISTS dept_schedule ("
        "dept_id INTEGER, "
        "day_of_week INTEGER, " // 0-6
        "shift_id INTEGER, "
        "PRIMARY KEY(dept_id, day_of_week), "
        "FOREIGN KEY(dept_id) REFERENCES departments(id) ON DELETE CASCADE, "
        "FOREIGN KEY(shift_id) REFERENCES shifts(id) ON DELETE SET NULL);";

    // (G) 用户特定日期排班表 (用于调休、加班或特定排班)
    const char* sql_user_sch = 
        "CREATE TABLE IF NOT EXISTS user_schedule ("
        "user_id INTEGER, "
        "date_str TEXT, "       // "YYYY-MM-DD"
        "shift_id INTEGER, "
        "PRIMARY KEY(user_id, date_str), "
        "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
        "FOREIGN KEY(shift_id) REFERENCES shifts(id) ON DELETE SET NULL);";

    // (H) 响铃计划表 (存储16组)
    const char* sql_bells = 
        "CREATE TABLE IF NOT EXISTS bells ("
        "id INTEGER PRIMARY KEY, "    // 固定ID 1-16
        "time TEXT, "                 // HH:MM
        "duration INTEGER, "          // 秒
        "days_mask INTEGER, "         // 位掩码
        "enabled INTEGER "            // 0/1
        ");";

    // 创建联合索引：加速 "查某人最近打卡" 和 "查某段时间记录"
    // 索引命名为 idx_att_user_time
    const char* sql_index = 
        "CREATE INDEX IF NOT EXISTS idx_att_user_time ON attendance(user_id, timestamp DESC);";

    bool ret = exec_sql(sql_dept, "Create Dept") && 
               exec_sql(sql_shifts, "Create Shifts V2") &&
               exec_sql(sql_bells, "Create Bells") &&
               exec_sql(sql_rules, "Create Rules") && 
               exec_sql(sql_users, "Create Users") &&
               exec_sql(sql_dept_sch, "Create Dept Schedule") && 
               exec_sql(sql_user_sch, "Create User Schedule") && 
               exec_sql(sql_att, "Create Attendance")&&
               exec_sql(sql_index, "Create Index");

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

// [辅助函数] 简单哈希转换 
static std::string simple_hash_password(const std::string& raw_pwd) {
    if (raw_pwd.empty()) return "";
    std::hash<std::string> hasher;
    size_t hash_val = hasher(raw_pwd);
    
    // 转为 hex 字符串存储
    std::stringstream ss;
    ss << std::hex << hash_val;
    return ss.str();
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

    // 2. 播种默认班次 
    if (is_table_empty("shifts")) {
        // 对应手册：班次一 (08:00-12:00, 14:00-18:00, 无加班)
        db_add_shift("Standard Shift", 
                     "08:00", "12:00",  // 上午
                     "14:00", "18:00",  // 下午
                     "", "",            // 无加班
                     0);
                     
        std::cout << "   [Seed] Created Standard Shift (Seg1: 08-12, Seg2: 14-18)." << std::endl;
    }

    //  3. 播种默认考勤规则
    if (is_table_empty("attendance_rules")) {
        // 默认允许迟到 15 分钟
        const char* sql = "INSERT INTO attendance_rules (company_name, late_threshold, early_leave_threshold) VALUES ('Smart Co.', 15, 0);";
        exec_sql(sql, "Seed Rules");
        std::cout << "   [Seed] Created default rules (Late Threshold: 15m)." << std::endl;
    }

    // 4. 播种默认管理员 (如果用户表为空)
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

    // 5. 初始化 16 组响铃槽位
    if (is_table_empty("bells")) {
        db_begin_transaction();
        for (int i = 1; i <= 16; i++) {
            // 默认：00:00, 响5秒, 全不选, 禁用
            std::string sql = "INSERT INTO bells (id, time, duration, days_mask, enabled) VALUES (" 
                            + std::to_string(i) + ", '00:00', 5, 0, 0);";
            exec_sql(sql.c_str(), "Seed Bell");
        }
        db_commit_transaction();
        std::cout << "   [Seed] Created 16 empty bell schedules." << std::endl;
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

bool db_update_shift(int shift_id, 
                     const std::string& s1_start, const std::string& s1_end,
                     const std::string& s2_start, const std::string& s2_end,
                     const std::string& s3_start, const std::string& s3_end,
                     int cross_day) {
    const char* sql = 
        "UPDATE shifts SET s1_start=?, s1_end=?, s2_start=?, s2_end=?, s3_start=?, s3_end=?, cross_day=? "
        "WHERE id=?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, s1_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, s1_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, s2_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, s2_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, s3_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, s3_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, cross_day);
    sqlite3_bind_int(stmt, 8, shift_id);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<ShiftInfo> db_get_shifts() {
    std::vector<ShiftInfo> list;
    sqlite3_stmt* stmt;
    
    const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day FROM shifts;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ShiftInfo s;
            s.id = sqlite3_column_int(stmt, 0);
            
            auto get_col_str = [&](int idx) -> std::string {
                const char* txt = (const char*)sqlite3_column_text(stmt, idx);
                return txt ? txt : "";
            };
            
            s.name = get_col_str(1);
            s.s1_start = get_col_str(2); s.s1_end = get_col_str(3);
            s.s2_start = get_col_str(4); s.s2_end = get_col_str(5);
            s.s3_start = get_col_str(6); s.s3_end = get_col_str(7);
            
            s.cross_day = sqlite3_column_int(stmt, 8);
            
            list.push_back(s);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

RuleConfig db_get_global_rules() {
    // 1. 设置默认值 (防止数据库字段为NULL或读取失败)
    RuleConfig config;
    config.company_name = "Smart Co.";
    config.late_threshold = 15;
    config.early_leave_threshold = 0;
    config.device_id = 1;
    config.volume = 70;
    config.screensaver_time = 0;
    config.max_admins = 10;
    config.relay_delay = 5;
    config.wiegand_fmt = 26;

    // 2. 查询所有字段
    const char* sql = "SELECT company_name, late_threshold, early_leave_threshold, "
                      "device_id, volume, screensaver_time, max_admins, relay_delay, wiegand_fmt "
                      "FROM attendance_rules LIMIT 1;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // 读取原有字段
            const char* name = (const char*)sqlite3_column_text(stmt, 0);
            config.company_name = name ? name : "Smart Co.";
            config.late_threshold = sqlite3_column_int(stmt, 1);
            config.early_leave_threshold = sqlite3_column_int(stmt, 2);

            // 读取新字段
            config.device_id = sqlite3_column_int(stmt, 3);
            config.volume = sqlite3_column_int(stmt, 4);
            config.screensaver_time = sqlite3_column_int(stmt, 5);
            config.max_admins = sqlite3_column_int(stmt, 6);
            config.relay_delay = sqlite3_column_int(stmt, 7);
            config.wiegand_fmt = sqlite3_column_int(stmt, 8);
        }
    }
    sqlite3_finalize(stmt);
    return config;
}

int db_add_shift(const std::string& name, 
                 const std::string& s1_start, const std::string& s1_end,
                 const std::string& s2_start, const std::string& s2_end,
                 const std::string& s3_start, const std::string& s3_end,
                 int cross_day) {
    const char* sql = 
        "INSERT INTO shifts (name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt* stmt;
    int new_id = -1;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        
        // 辅助 lambda：空字符串存 NULL 或 空串? 这里存空串即可
        auto bind_str = [&](int idx, const std::string& s) {
            sqlite3_bind_text(stmt, idx, s.c_str(), -1, SQLITE_STATIC);
        };
        
        bind_str(2, s1_start); bind_str(3, s1_end);
        bind_str(4, s2_start); bind_str(5, s2_end);
        bind_str(6, s3_start); bind_str(7, s3_end);
        
        sqlite3_bind_int(stmt, 8, cross_day);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            new_id = (int)sqlite3_last_insert_rowid(db);
        }
    }
    sqlite3_finalize(stmt);
    return new_id;
}

bool db_delete_shift(int shift_id) {
    const char* sql = "DELETE FROM shifts WHERE id=?;";
    sqlite3_stmt* stmt;

    // 1. 准备语句
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Delete Shift Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 2. 绑定参数 (shift_id 绑定到第一个 ?)
    sqlite3_bind_int(stmt, 1, shift_id);
    
    // 3. 执行语句
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    
    if (!ok) {
        std::cerr << "[Data] Delete Shift Execution Failed: " << sqlite3_errmsg(db) << std::endl;
    }

    // 4. 释放资源
    sqlite3_finalize(stmt);
    return ok;
}

bool db_update_global_rules(const RuleConfig& config) {
    // 强制更新 id=1 的记录
    const char* sql = "UPDATE attendance_rules SET "
                      "company_name=?, late_threshold=?, early_leave_threshold=?, "
                      "device_id=?, volume=?, screensaver_time=?, max_admins=?, "
                      "relay_delay=?, wiegand_fmt=? "
                      "WHERE id=1;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update Rules Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 绑定原有参数
    sqlite3_bind_text(stmt, 1, config.company_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, config.late_threshold);
    sqlite3_bind_int(stmt, 3, config.early_leave_threshold);

    // [新增] 绑定新参数
    sqlite3_bind_int(stmt, 4, config.device_id);
    sqlite3_bind_int(stmt, 5, config.volume);
    sqlite3_bind_int(stmt, 6, config.screensaver_time);
    sqlite3_bind_int(stmt, 7, config.max_admins);
    sqlite3_bind_int(stmt, 8, config.relay_delay);
    sqlite3_bind_int(stmt, 9, config.wiegand_fmt);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if(ok) {
        std::cout << "[Data] System Config Updated." << std::endl;
    }
    return ok;
}

// ================= 3. 用户管理 DAO (升级版) =================

int db_add_user(const UserData& info, const cv::Mat& face_img) {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 确保线程安全
    
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

    sqlite3_bind_text(stmt, 1, info.name.c_str(), -1, SQLITE_STATIC);
    // 2. 绑定参数
    std::string hashed_pwd = simple_hash_password(info.password); 
    sqlite3_bind_text(stmt, 2, hashed_pwd.c_str(), -1, SQLITE_TRANSIENT);
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
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 确保线程安全

    UserData u;
    u.id = 0; // 0 表示无效/未找到
    
    const char* sql = 
        "SELECT u.id, u.name, u.password, u.card_id, u.privilege, u.dept_id, "
        "u.face_data, u.fingerprint_data, d.name "
        "FROM users u "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE u.id=?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // [0] ID
            u.id = sqlite3_column_int(stmt, 0);
            
            // [1] Name
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            u.name = name ? name : "";
            
            // [2] Password
            const char* pwd = (const char*)sqlite3_column_text(stmt, 2);
            u.password = pwd ? pwd : "";
            
            // [3] Card ID
            const char* card = (const char*)sqlite3_column_text(stmt, 3);
            u.card_id = card ? card : "";
            
            // [4] Role (DB字段是 privilege)
            u.role = sqlite3_column_int(stmt, 4);
            
            // [5] Dept ID
            u.dept_id = sqlite3_column_int(stmt, 5);
            
            // [6] Face Data (人脸数据)
            const void* face_blob = sqlite3_column_blob(stmt, 6);
            int face_bytes = sqlite3_column_bytes(stmt, 6);
            if (face_blob && face_bytes > 0) {
                const uint8_t* ptr = (const uint8_t*)face_blob;
                u.face_feature.assign(ptr, ptr + face_bytes);
            }

            // [7] Fingerprint Data (指纹数据)
            const void* fp_blob = sqlite3_column_blob(stmt, 7);
            int fp_bytes = sqlite3_column_bytes(stmt, 7);
            if (fp_blob && fp_bytes > 0) {
                const uint8_t* ptr = (const uint8_t*)fp_blob;
                u.fingerprint_feature.assign(ptr, ptr + fp_bytes);
            }

            // [8] Dept Name (部门名称) - 修复部门不显示的问题
            const char* dname = (const char*)sqlite3_column_text(stmt, 8);
            u.dept_name = dname ? dname : "Unknown"; // 如果没部门，显示Unknown
        }
    } else {
        std::cerr << "[Data] Get User Info SQL Error: " << sqlite3_errmsg(db) << std::endl;
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
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 确保线程安全

    std::vector<UserData> users;
    // 使用 LEFT JOIN 关联查询 departments 表，获取 dept_name
    // 假设你的部门表叫 departments，字段是 id 和 name
    const char* sql = "SELECT u.id, u.name, u.dept_id, u.privilege, u.password, u.card_id, u.face_data, d.name "
                  "FROM users u "
                  "LEFT JOIN departments d ON u.dept_id = d.id";
                      
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        return users;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserData u;
        u.id = sqlite3_column_int(stmt, 0);
        u.name = (const char*)sqlite3_column_text(stmt, 1);
        u.dept_id = sqlite3_column_int(stmt, 2);
        u.role = sqlite3_column_int(stmt, 3);
        
        const char* pwd = (const char*)sqlite3_column_text(stmt, 4);
        u.password = pwd ? pwd : "";
        
        const char* card = (const char*)sqlite3_column_text(stmt, 5);
        u.card_id = card ? card : "";

        const void* blob = sqlite3_column_blob(stmt, 6);
        int bytes = sqlite3_column_bytes(stmt, 6);
        if (bytes > 0 && blob) {
             const uint8_t* ptr = (const uint8_t*)blob;
             u.face_feature.assign(ptr, ptr + bytes);
        }

        // 获取关联查询出来的部门名称 (第7列，索引从0开始是7)
        const char* d_name = (const char*)sqlite3_column_text(stmt, 7);
        u.dept_name = d_name ? d_name : "Unknown"; // 如果查不到部门，显示 Unknown

        users.push_back(u);
    }
    sqlite3_finalize(stmt);
    return users;
}

bool db_assign_user_shift(int user_id, int shift_id) {
    const char* sql = "UPDATE users SET default_shift_id=? WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    if (shift_id > 0) sqlite3_bind_int(stmt, 1, shift_id);
    else sqlite3_bind_null(stmt, 1); // 解除排班
    
    sqlite3_bind_int(stmt, 2, user_id);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

ShiftInfo db_get_user_shift(int user_id) {
    ShiftInfo s = {0, "", "", "", "", "", "", "", 0}; // 初始化空结构
    
    const char* sql = 
        "SELECT s.id, s.name, "
        "s.s1_start, s.s1_end, s.s2_start, s.s2_end, s.s3_start, s.s3_end, s.cross_day "
        "FROM users u "
        "JOIN shifts s ON u.default_shift_id = s.id "
        "WHERE u.id = ?;";
        
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.id = sqlite3_column_int(stmt, 0);
            s.name = (const char*)sqlite3_column_text(stmt, 1);
            
            // 提取所有时段
            auto get_col = [&](int i){ const char* t = (const char*)sqlite3_column_text(stmt, i); return t?t:""; };
            s.s1_start = get_col(2); s.s1_end = get_col(3);
            s.s2_start = get_col(4); s.s2_end = get_col(5);
            s.s3_start = get_col(6); s.s3_end = get_col(7);
            
            s.cross_day = sqlite3_column_int(stmt, 8);
        }
    }
    sqlite3_finalize(stmt);
    return s;
}

//  用户信息修改接口 (不含密码)
bool db_update_user_basic(int user_id, const std::string& name, int dept_id, int privilege, const std::string& card_id) {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 加锁保护

    const char* sql = 
        "UPDATE users SET name=?, dept_id=?, privilege=?, card_id=? WHERE id=?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update User Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 绑定参数
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    
    if (dept_id > 0) sqlite3_bind_int(stmt, 2, dept_id);
    else sqlite3_bind_null(stmt, 2); // 部门为空
    
    sqlite3_bind_int(stmt, 3, privilege);
    sqlite3_bind_text(stmt, 4, card_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, user_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (ok) std::cout << "[Data] User " << user_id << " info updated." << std::endl;
    return ok;
}

// 用户密码更新接口
bool db_update_user_password(int user_id, const std::string& new_raw_password) {
    const char* sql = "UPDATE users SET password=? WHERE id=?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    // 1. 对新密码进行哈希处理 (复用现有的哈希函数)
    std::string hashed_pwd = simple_hash_password(new_raw_password);

    // 2. 绑定参数
    sqlite3_bind_text(stmt, 1, hashed_pwd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (ok) std::cout << "[Data] User " << user_id << " password updated." << std::endl;
    return ok;
}

// ================= 4. 考勤记录 DAO =================

bool db_log_attendance(int user_id, int shift_id, const cv::Mat& image, int status) {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 加锁保护

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

// [Phase 05 新增] 实现获取最后打卡时间
time_t db_getLastPunchTime(int user_id) {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 加锁保护

    if (!db) return 0;

    sqlite3_stmt* stmt;
    // 查询该用户最新的打卡记录时间
    const char* sql = "SELECT timestamp FROM attendance WHERE user_id = ? ORDER BY timestamp DESC LIMIT 1;";
    time_t last_ts = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            last_ts = (time_t)sqlite3_column_int64(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return last_ts;
}

std::vector<AttendanceRecord> db_get_records(long long start_ts, long long end_ts) {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 加锁保护

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

// ================= 5.数据库事务接口 =================
bool db_begin_transaction() {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 确保线程安全
    
    return exec_sql("BEGIN TRANSACTION;", "Tx Begin");
}

bool db_commit_transaction() {

    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 确保线程安全

    return exec_sql("COMMIT;", "Tx Commit");
}

// ================= 6. 排班管理接口实现 =================

// 辅助函数：将时间戳转换为 YYYY-MM-DD
static std::string timestamp_to_date(long long ts) {
    std::time_t t = (std::time_t)ts;
    struct tm* tm_info = std::localtime(&t);
    char buffer[20];
    std::strftime(buffer, 20, "%Y-%m-%d", tm_info);
    return std::string(buffer);
}

// 辅助函数：获取星期几 (0=Sun, 1=Mon...)
static int timestamp_to_weekday(long long ts) {
    std::time_t t = (std::time_t)ts;
    struct tm* tm_info = std::localtime(&t);
    return tm_info->tm_wday;
}

// 辅助函数：根据ID获取班次详情 (内部复用)
static ShiftInfo get_shift_by_id(int shift_id) {
    ShiftInfo s = {0, "", "", "", "", "", "", "", 0};
    if (shift_id <= 0) return s; // ID<=0 视为休息

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day FROM shifts WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, shift_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.id = sqlite3_column_int(stmt, 0);
            s.name = (const char*)sqlite3_column_text(stmt, 1);
            auto get_col = [&](int i){ const char* t = (const char*)sqlite3_column_text(stmt, i); return t?t:""; };
            s.s1_start = get_col(2); s.s1_end = get_col(3);
            s.s2_start = get_col(4); s.s2_end = get_col(5);
            s.s3_start = get_col(6); s.s3_end = get_col(7);
            s.cross_day = sqlite3_column_int(stmt, 8);
        }
    }
    sqlite3_finalize(stmt);
    return s;
}

bool db_set_dept_schedule(int dept_id, int day_of_week, int shift_id) {
    // 使用 INSERT OR REPLACE (UPSERT)
    const char* sql = "INSERT OR REPLACE INTO dept_schedule (dept_id, day_of_week, shift_id) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, dept_id);
    sqlite3_bind_int(stmt, 2, day_of_week);
    sqlite3_bind_int(stmt, 3, shift_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool db_set_user_special_schedule(int user_id, const std::string& date_str, int shift_id) {
    const char* sql = "INSERT OR REPLACE INTO user_schedule (user_id, date_str, shift_id) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, date_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, shift_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// [核心] 智能排班查询
ShiftInfo db_get_user_shift_smart(int user_id, long long timestamp) {
    
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);// 确保线程安全

    int final_shift_id = 0;
    
    std::string date_str = timestamp_to_date(timestamp);
    int weekday = timestamp_to_weekday(timestamp);
    
    // 1. 优先级最高：检查个人特殊排班 (User Schedule)
    {
        const char* sql = "SELECT shift_id FROM user_schedule WHERE user_id=? AND date_str=?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, date_str.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                final_shift_id = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                return get_shift_by_id(final_shift_id); // 命中直接返回
            }
        }
        sqlite3_finalize(stmt);
    }

    // 2. 优先级第二：检查部门周排班 (Dept Schedule)
    // 先查用户属于哪个部门
    int dept_id = 0;
    {
        UserData u = db_get_user_info(user_id);
        dept_id = u.dept_id;
    }
    
    if (dept_id > 0) {
        const char* sql = "SELECT shift_id FROM dept_schedule WHERE dept_id=? AND day_of_week=?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, dept_id);
            sqlite3_bind_int(stmt, 2, weekday);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                final_shift_id = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                return get_shift_by_id(final_shift_id); // 命中直接返回
            }
        }
        sqlite3_finalize(stmt);
    }

    // 3. 优先级最低：使用用户默认班次 (User Default / Fallback)
    // 复用之前的逻辑，从 users 表拿 default_shift_id
    // 注意：这里我们不再调用 db_get_user_shift，而是直接查表，避免循环调用
    {
        const char* sql = "SELECT default_shift_id FROM users WHERE id=?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                final_shift_id = sqlite3_column_int(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
    }

    return get_shift_by_id(final_shift_id);
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

/**
 * @brief 获取最后保存图像的ID
 * @return 最后保存图像的ID，失败返回 -1
 */
long long data_getLastImageID() {
    if (!db) {
        std::cerr << "[Data] Error: Database not initialized!" << std::endl;
        return -1;
    }// 校验数据库连接

    const char* sql = "SELECT id FROM attendance ORDER BY id DESC LIMIT 1;";// 获取最后一条记录的ID
    sqlite3_stmt* stmt;// 语句句柄
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);// 预编译 SQL
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }// 预编译失败

    rc = sqlite3_step(stmt);// 执行 SQL
    long long last_id = -1;// 默认返回值
    if (rc == SQLITE_ROW) {
        last_id = sqlite3_column_int64(stmt, 0);
    }// 成功获取到数据 
    else {
        std::cerr << "[Data] No images found in database." << std::endl;
    }// 未找到数据

    sqlite3_finalize(stmt);// 清理语句句柄
    return last_id;// 返回最后保存的ID
}

// ================= Epic 4.3 系统维护接口 =================

bool db_clear_attendance() {
    std::cout << "[Data] Clearing all attendance records..." << std::endl;
    // 清空表数据
    bool ret = exec_sql("DELETE FROM attendance;", "Clear Att") &&
               exec_sql("DELETE FROM sqlite_sequence WHERE name='attendance';", "Reset Seq");
    
    // 清空图片文件夹
    if (ret) {
        try {
            fs::remove_all(IMAGE_DIR); // 删除目录
            fs::create_directories(IMAGE_DIR); // 重建目录
        } catch (...) {
            return false;
        }
    }
    return ret;
}

bool db_clear_users() {
    // 直接使用本文件顶部的静态全局变量 db
    if (!db) return false; 
    
    const char* sql = "DELETE FROM users;";
    char* errMsg = nullptr;
    
    //  使用 db 而不是 g_db
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        if (errMsg) {
            std::cerr << "[Data] Clear Users Error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        return false;
    }
    
    // 如果启用了级联删除，考勤记录会自动删除
    // 如果没有，可能需要额外执行 DELETE FROM attendance;
    
    std::cout << "[Data] All users cleared." << std::endl;
    return true;
}

bool db_factory_reset() {
    std::cout << "[Data] !!! FACTORY RESET !!!" << std::endl;
    data_close(); // 先断开连接
    
    try {
        if (fs::exists(DB_NAME)) fs::remove(DB_NAME);
        if (fs::exists(IMAGE_DIR)) fs::remove_all(IMAGE_DIR);
    } catch (...) {}

    // 重新初始化（会自动建表和播种默认用户）
    return data_init();
}

// =================  铃声管理接口 =================
std::vector<BellSchedule> db_get_all_bells() {
    std::vector<BellSchedule> list;
    const char* sql = "SELECT id, time, duration, days_mask, enabled FROM bells ORDER BY id ASC;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BellSchedule b;
            b.id = sqlite3_column_int(stmt, 0);
            b.time = (const char*)sqlite3_column_text(stmt, 1);
            b.duration = sqlite3_column_int(stmt, 2);
            b.days_mask = sqlite3_column_int(stmt, 3);
            b.enabled = sqlite3_column_int(stmt, 4);
            list.push_back(b);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

// 更新铃声设置
bool db_update_bell(const BellSchedule& bell) {
    const char* sql = "UPDATE bells SET time=?, duration=?, days_mask=?, enabled=? WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, bell.time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, bell.duration);
    sqlite3_bind_int(stmt, 3, bell.days_mask);
    sqlite3_bind_int(stmt, 4, bell.enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 5, bell.id);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
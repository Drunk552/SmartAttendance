/**
 * @file database.cpp
 * @brief 管理旧 SQLite 数据库连接、Schema 初始化和播种生命周期
 * @details 具体业务 CRUD 已按职责迁入 storage/sqlite，本文件不提供 Repository。
 */

#include "storage/database.h"
#include "data/db_storage.h"
#include "storage/sqlite/legacy_db_internal.h"
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <filesystem> // C++17 标准库，用于处理文件系统
#include <sys/stat.h>
#include <functional> // 引入哈希支持
#include <sstream>
#include <iomanip>
#include <limits>
#include <mutex>// 用于线程安全
#include <shared_mutex>

namespace fs = std::filesystem;

// 配置：打卡图片存储的文件夹名称
extern const std::string IMAGE_DIR = "captured_images";
//配置： 注册时的人脸图片存储的文件夹名称
extern const std::string AVATAR_DIR = "registered_avatars";
// 配置：数据库文件名
extern const std::string DB_NAME = "attendance.db";

sqlite3* db = nullptr;// 数据库连接句柄 (静态全局变量，仅本文件可见)

sqlite3_stmt* g_stmt_log_attendance = nullptr;// 预编译语句缓存

std::shared_mutex g_db_mutex;// 读写锁：它把锁分为了两种形态
//1.共享锁（读锁）：允许多个线程同时拿到锁。只要大家都是来“读”数据的，请随意进出，绝不阻塞！
//2.排他锁（写锁）：非常霸道。只要有人要“写”数据，它就会把门反锁，直到写完为止。


// ================= 辅助函数 (Helpers)将 Mat 序列化为 vector<uchar> (用于存 BLOB) =================

/**
 * @brief 检查时间字符串是否表示空值（无考勤要求）
 * @param time_str 时间字符串
 * @return true 表示空值（"--:--"或空字符串）
 * @note 业务文档规定："--:--"代表无考勤要求
 */
bool is_time_empty(const std::string& time_str) {
    return time_str.empty() || time_str == "--:--";
}

/**
 * @brief 标准化时间字符串
 * @param time_str 原始时间字符串
 * @return 标准化后的时间字符串，空值返回"--:--"
 * @note 用于数据持久化前的格式化
 */
std::string normalize_time_string(const std::string& time_str) {
    if (time_str.empty() || time_str == "--:--") {
        return "--:--";
    }
    // 验证格式是否为HH:MM，这里简单返回原值
    // 业务层应进行更严格的验证
    return time_str;
}

/**
 * @brief 执行无返回值的 SQL 语句
 * @param sql SQL 命令字符串
 * @param tag用于日志报错的标签
 */
bool exec_sql(const char* sql, const char* tag) {
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
    // 确保存储目录存在
    try {
        if (!fs::exists(IMAGE_DIR)) fs::create_directories(IMAGE_DIR);
    } catch (const std::exception& e) {
        std::cerr << "[Data] FS Init Error: " << e.what() << std::endl;
        return false;
    }

    // 连接数据库
    if (sqlite3_open(DB_NAME.c_str(), &db)) {
        std::cerr << "[Data] Can't open DB: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // ================= 性能调优：SQLite Pragmas =================
    std::cout << "[DB] Applying performance pragmas..." << std::endl;
    // 1. 启用 WAL 模式 (极大提升读写并发性能，读写不互斥)
    exec_sql("PRAGMA journal_mode=WAL;", "Enable WAL mode");
    // 2. 调整同步模式 (WAL模式下 NORMAL 既安全又快)
    exec_sql("PRAGMA synchronous=NORMAL;", "Set synchronous to NORMAL");
    // 3. 将临时表和索引放在内存中，减少磁盘 IO
    exec_sql("PRAGMA temp_store=MEMORY;", "Set temp_store to MEMORY");
    // 4. 增加缓存大小 (例如 -20000 表示分配约 20MB 内存做缓存)
    exec_sql("PRAGMA cache_size=-20000;", "Set cache size");
    // 5. 开启外键约束 (确保部门和班次的外键关联生效)
    exec_sql("PRAGMA foreign_keys=ON;", "Enable foreign keys");
    // ===================================================================

    // 创建/更新表结构

    // (A) 公司表 - 支持多公司架构
    const char* sql_companies =
        "CREATE TABLE IF NOT EXISTS companies ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL UNIQUE, "          // 公司名称
        "code TEXT, "                          // 公司代码/简称
        "address TEXT, "                       // 公司地址
        "contact_phone TEXT, "                 // 联系电话
        "contact_email TEXT, "                 // 联系邮箱
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP, "
        "updated_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

    // (B) 部门表 - 添加 company_id 外键
    const char* sql_dept =
        "CREATE TABLE IF NOT EXISTS departments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "company_id INTEGER NOT NULL, "        // 所属公司ID
        "FOREIGN KEY(company_id) REFERENCES companies(id) ON DELETE CASCADE, "
        "UNIQUE(company_id, name)"             // 同一公司内部门名称唯一
        ");";

    // (C) 班次表
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
        "device_id INTEGER DEFAULT 1, "
        "volume INTEGER DEFAULT 70, "
        "screensaver_time INTEGER DEFAULT 0, " // 0=关闭
        "max_admins INTEGER DEFAULT 10, "
        "duplicate_punch_limit INTEGER DEFAULT 3, " // 默认3分钟防重复
        "language TEXT DEFAULT 'zh-CN', "           // 默认中文
        "date_format TEXT DEFAULT 'YYYY-MM-DD', "   // 默认日期格式
        "return_home_delay INTEGER DEFAULT 30, "    // 默认30秒退回主界面
        "warning_record_count INTEGER DEFAULT 99, "   // 默认警告阈值99条
        "relay_delay INTEGER DEFAULT 5, "      // 默认开门5秒
        "wiegand_fmt INTEGER DEFAULT 26, "     // 默认韦戩26
        // 【流程图节点K】周末上班规则：0=不上班，1=上班
        "sat_work INTEGER DEFAULT 0, "        // 星期六是否上班 (默认不上班)
        "sun_work INTEGER DEFAULT 0 "         // 星期日是否上班 (默认不上班)
        ");";
    // 兼容旧数据库：如果列不存在则自动添加
    exec_sql("ALTER TABLE attendance_rules ADD COLUMN sat_work INTEGER DEFAULT 0;", nullptr);
    exec_sql("ALTER TABLE attendance_rules ADD COLUMN sun_work INTEGER DEFAULT 0;", nullptr);

    // (D) 用户表 (包含权限、密码、关联部门、人脸BLOB)
    const char* sql_users =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "password TEXT, "
        "card_id TEXT, "
        "privilege INTEGER DEFAULT 0, " // 0:User, 1:Admin
        "face_feature BLOB, "           // 人脸二进制数据，对应UserData.face_feature
        "avatar_path TEXT, "            // 用来存用户人脸照的本地文件路径
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

    // (I) 系统全局配置表：采用简单的 Key-Value 结构
    const char* sql_system_config =
        "CREATE TABLE IF NOT EXISTS system_config ("
        "config_key TEXT PRIMARY KEY, "
        "config_value TEXT"
        ");";

    // (J) 全局节假日表：以日期为主键
    const char* sql_holidays =
        "CREATE TABLE IF NOT EXISTS holidays ("
        "date_str TEXT PRIMARY KEY, "
        "name TEXT NOT NULL"
        ");";

    // 创建联合索引：加速 "查某人最近打卡" 和 "查某段时间记录"
    // 索引命名为 idx_att_user_time
    const char* sql_index =
        "CREATE INDEX IF NOT EXISTS idx_att_user_time ON attendance(user_id, timestamp DESC);";

    // 创建部门-公司联合索引：加速按公司查询部门
    const char* sql_dept_company_idx =
        "CREATE INDEX IF NOT EXISTS idx_dept_company ON departments(company_id);";

    bool ret = exec_sql(sql_companies, "Create Companies") &&
               exec_sql(sql_dept, "Create Dept") &&
               exec_sql(sql_shifts, "Create Shifts V2") &&
               exec_sql(sql_bells, "Create Bells") &&
               exec_sql(sql_system_config, "Create System Config") &&
               exec_sql(sql_holidays, "Create Holidays") &&
               exec_sql(sql_rules, "Create Rules") &&
               exec_sql(sql_users, "Create Users") &&
               exec_sql(sql_dept_sch, "Create Dept Schedule") &&
               exec_sql(sql_user_sch, "Create User Schedule") &&
               exec_sql(sql_att, "Create Attendance")&&
               exec_sql(sql_index, "Create Index") &&
               exec_sql(sql_dept_company_idx, "Create Dept Company Index");

    if (ret) {
        std::cout << "[Data] DAO Layer Initialized." << std::endl;
        // 执行播种
        data_seed();

        // 预编译高频使用的插入打卡记录语句，并存入全局变量
        const char* sql_log = "INSERT INTO attendance (user_id, shift_id, image_path, timestamp, status) VALUES (?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql_log, -1, &g_stmt_log_attendance, nullptr) != SQLITE_OK) {
            std::cerr << "[Data] Warning: Failed to precompile log_attendance statement: " << sqlite3_errmsg(db) << std::endl;
        } else {
            std::cout << "[Data] Precompiled log_attendance statement successfully." << std::endl;
        }
    }

    return ret;

}

bool data_is_open() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    return db != nullptr;
}

// [辅助函数] 检查表中是否有数据
static bool is_table_empty(const char* table_name) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁
    ScopedSqliteStmt stmt;

    std::string sql = "SELECT COUNT(*) FROM " + std::string(table_name) + ";";
    int count = 0;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, stmt.ptr(), 0) == SQLITE_OK) {
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt.get(), 0);
        }
    }

    return (count == 0);
}

// [辅助函数] 简单哈希转换
std::string db_hash_password(const std::string& raw_pwd) {
    if (raw_pwd.empty()) return "";
    std::hash<std::string> hasher;
    size_t hash_val = hasher(raw_pwd);

    // 转为 hex 字符串存储
    std::stringstream ss;
    ss << std::hex << hash_val;
    return ss.str();
}


// =================  数据播种 =================

bool data_seed() {

    std::cout << ">>> [Data] Checking for data seeding..." << std::endl;

    // 0. 播种默认公司 (如果为空)
    if (is_table_empty("companies")) {
        const char* sql = "INSERT INTO companies (id, name, code, address) VALUES (1, '777', 'SmartAtt', '中国');";
        if (exec_sql(sql, "Seed Company")) {
            std::cout << "   [Seed] Created default company: '777'" << std::endl;
        }
    }

    // 1. 播种默认部门 (如果为空)
    if (is_table_empty("departments")) {
        // 重置 AUTOINCREMENT 计数器，确保 id 从 1 开始
        exec_sql("DELETE FROM sqlite_sequence WHERE name='departments';", "Reset Dept Sequence");

        // 1. 插入必须存在的默认部门（ID: 1）
        exec_sql("INSERT INTO departments (id, name, company_id) VALUES (1, 'Not Set 1', 1);", "Seed Dept Not Set");

        // 2. 循环插入额外的 15 个部门（总计 16 条）
        const char* predefined_names[] = {
            "Not Set 2", "Not Set 3", "Not Set 4", "Not Set 5", "Not Set 6",
            "Not Set 7", "Not Set 8", "Not Set 9", "Not Set 10", "Not Set 11",
            "Not Set 12", "Not Set 13", "Not Set 14", "Not Set 15", "Not Set 16"
        };

        for (int i = 2; i <= 16; ++i) {
            // 获取数组中的部门名称（i-2 是因为从索引 0 开始）
            std::string dept_name = predefined_names[i - 2];

            // 拼接 SQL 语句
            std::string sql = "INSERT INTO departments (id, name, company_id) VALUES ("
                              + std::to_string(i) + ", '" + dept_name + "', 1);";

            // 执行 SQL
            exec_sql(sql.c_str(), ("Seed Dept " + dept_name).c_str());
        }

        std::cout << "   [Seed] Created 16 default departments for company ID: 1" << std::endl;
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
        // 默认允许迟到 15 分钟；周六/周日默认不上班（流程图节点K缺省安全封闭）
        const char* sql = "INSERT INTO attendance_rules (company_name, late_threshold, early_leave_threshold, sat_work, sun_work) VALUES ('Smart Co.', 15, 0, 0, 0);";
        exec_sql(sql, "Seed Rules");
        std::cout << "   [Seed] Created default rules (Late Threshold: 15m, Sat/Sun off)." << std::endl;
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

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    // 释放预编译语句的内存
    if (g_stmt_log_attendance) {
        sqlite3_finalize(g_stmt_log_attendance);
        g_stmt_log_attendance = nullptr;
    }

    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "[Data] Database connection closed." << std::endl;
    }
}

/**
 * @file db_storage.cpp
 * @brief 数据层实现
 * @details 实现了数据库初始化、表结构升级以及部门、班次、用户、考勤的 CRUD 操作。
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
#include <shared_mutex>

namespace fs = std::filesystem;

// 配置：打卡图片存储的文件夹名称
const std::string IMAGE_DIR = "captured_images";
//配置： 注册时的人脸图片存储的文件夹名称
const std::string AVATAR_DIR = "registered_avatars";
// 配置：数据库文件名
const std::string DB_NAME = "attendance.db";

static sqlite3* db = nullptr;// 数据库连接句柄 (静态全局变量，仅本文件可见)

static sqlite3_stmt* g_stmt_log_attendance = nullptr;// 预编译语句缓存

static std::shared_mutex g_db_mutex;// 读写锁：它把锁分为了两种形态
//1.共享锁（读锁）：允许多个线程同时拿到锁。只要大家都是来“读”数据的，请随意进出，绝不阻塞！
//2.排他锁（写锁）：非常霸道。只要有人要“写”数据，它就会把门反锁，直到写完为止。


// ================= RAII 封装：自动管理 sqlite3_stmt 的生命周期 =================

class ScopedSqliteStmt {
private:
    sqlite3_stmt* stmt;
public:
    // 构造函数，初始化为 nullptr 或传入的指针
    ScopedSqliteStmt(sqlite3_stmt* s = nullptr) : stmt(s) {}
    
    // 析构函数，离开作用域时自动调用 sqlite3_finalize
    ~ScopedSqliteStmt() {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
    
    // 获取底层指针，用于 sqlite3_step, sqlite3_bind_* 等操作
    sqlite3_stmt* get() const { return stmt; }
    
    // 获取指针的地址，专门用于 sqlite3_prepare_v2 函数接收分配的 stmt
    sqlite3_stmt** ptr() { return &stmt; }
    
    // 核心安全机制：禁用拷贝构造和赋值，防止两个对象清理同一个 stmt (Double Free)
    ScopedSqliteStmt(const ScopedSqliteStmt&) = delete;
    ScopedSqliteStmt& operator=(const ScopedSqliteStmt&) = delete;
};

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
        "face_data BLOB, "              // 人脸二进制数据
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

    bool ret = exec_sql(sql_dept, "Create Dept") && 
               exec_sql(sql_shifts, "Create Shifts V2") &&
               exec_sql(sql_bells, "Create Bells") &&
               exec_sql(sql_system_config, "Create System Config") && 
               exec_sql(sql_holidays, "Create Holidays") &&
               exec_sql(sql_rules, "Create Rules") && 
               exec_sql(sql_users, "Create Users") &&
               exec_sql(sql_dept_sch, "Create Dept Schedule") && 
               exec_sql(sql_user_sch, "Create User Schedule") && 
               exec_sql(sql_att, "Create Attendance")&&
               exec_sql(sql_index, "Create Index");
    
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

// ================= 1. 部门管理 DAO =================

bool db_add_department(const std::string& dept_name) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    ScopedSqliteStmt stmt;

    const char* sql = "INSERT INTO departments (name) VALUES (?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt.get(), 1, dept_name.c_str(), -1, SQLITE_STATIC);
    
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

std::vector<DeptInfo> db_get_departments() {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<DeptInfo> list;
    ScopedSqliteStmt stmt;

    const char* sql = "SELECT id, name FROM departments;";
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            DeptInfo d;
            d.id = sqlite3_column_int(stmt.get(), 0);
            const char* txt = (const char*)sqlite3_column_text(stmt.get(), 1);
            d.name = txt ? txt : "";
            list.push_back(d);
        }
    }

    return list;
}

bool db_delete_department(int dept_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "DELETE FROM departments WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt.get(), 1, dept_id);
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

// ================= 2. 班次管理 DAO =================

bool db_update_shift(int shift_id, 
                     const std::string& s1_start, const std::string& s1_end,
                     const std::string& s2_start, const std::string& s2_end,
                     const std::string& s3_start, const std::string& s3_end,
                     int cross_day) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）
    
    const char* sql = 
        "UPDATE shifts SET s1_start=?, s1_end=?, s2_start=?, s2_end=?, s3_start=?, s3_end=?, cross_day=? "
        "WHERE id=?;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt.get(), 1, s1_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, s1_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 3, s2_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 4, s2_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 5, s3_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 6, s3_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 7, cross_day);
    sqlite3_bind_int(stmt.get(), 8, shift_id);
    
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

std::vector<ShiftInfo> db_get_shifts() {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<ShiftInfo> list;
    ScopedSqliteStmt stmt;
    
    const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day FROM shifts;";
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            ShiftInfo s;
            s.id = sqlite3_column_int(stmt.get(), 0);
            
            auto get_col_str = [&](int idx) -> std::string {
                const char* txt = (const char*)sqlite3_column_text(stmt.get(), idx);
                return txt ? txt : "";
            };
            
            s.name = get_col_str(1);
            s.s1_start = get_col_str(2); s.s1_end = get_col_str(3);
            s.s2_start = get_col_str(4); s.s2_end = get_col_str(5);
            s.s3_start = get_col_str(6); s.s3_end = get_col_str(7);
            
            s.cross_day = sqlite3_column_int(stmt.get(), 8);
            
            list.push_back(s);
        }
    }

    return list;
}

// 根据班次 ID 获取班次详细信息
std::optional<ShiftInfo> db_get_shift_info(int shift_id) {
    // 加上你的共享读锁，确保并发安全
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);

    // 查询 shifts 表中该 ID 对应的四个时间段
    const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end FROM shifts WHERE id = ?;";
    
    // 使用你封装的语句管理对象
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get Shift Info Failed: " << sqlite3_errmsg(db) << std::endl;
        return std::nullopt;
    }

    // 绑定参数
    sqlite3_bind_int(stmt.get(), 1, shift_id);

    // 执行查询
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        ShiftInfo shift;
        shift.id = sqlite3_column_int(stmt.get(), 0);
        
        const char* name = (const char*)sqlite3_column_text(stmt.get(), 1);
        shift.name = name ? name : "";
        
        const char* s1_start = (const char*)sqlite3_column_text(stmt.get(), 2);
        shift.s1_start = s1_start ? s1_start : "";
        
        const char* s1_end = (const char*)sqlite3_column_text(stmt.get(), 3);
        shift.s1_end = s1_end ? s1_end : "";
        
        const char* s2_start = (const char*)sqlite3_column_text(stmt.get(), 4);
        shift.s2_start = s2_start ? s2_start : "";
        
        const char* s2_end = (const char*)sqlite3_column_text(stmt.get(), 5);
        shift.s2_end = s2_end ? s2_end : "";
        
        return shift; // 找到数据，自动打包进 optional
    }

    // 没有这一行数据，返回空
    return std::nullopt;
}

RuleConfig db_get_global_rules() {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

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
                  "device_id, volume, screensaver_time, max_admins, relay_delay, wiegand_fmt, "
                  "duplicate_punch_limit, language, date_format, return_home_delay, warning_record_count, "
                  "sat_work, sun_work " // 【流程图节点K】周末上班规则
                  "FROM attendance_rules LIMIT 1;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            // 读取字段
            const char* name = (const char*)sqlite3_column_text(stmt.get(), 0);
            config.company_name = name ? name : "Smart Co.";
            config.late_threshold = sqlite3_column_int(stmt.get(), 1);
            config.early_leave_threshold = sqlite3_column_int(stmt.get(), 2);
            config.device_id = sqlite3_column_int(stmt.get(), 3);
            config.volume = sqlite3_column_int(stmt.get(), 4);
            config.screensaver_time = sqlite3_column_int(stmt.get(), 5);
            config.max_admins = sqlite3_column_int(stmt.get(), 6);
            
            config.relay_delay = sqlite3_column_int(stmt.get(), 7); 
            config.wiegand_fmt = sqlite3_column_int(stmt.get(), 8); 

            config.duplicate_punch_limit = sqlite3_column_int(stmt.get(), 9);
            
            const char* lang = (const char*)sqlite3_column_text(stmt.get(), 10);
            config.language = lang ? lang : "zh-CN";
            
            const char* df = (const char*)sqlite3_column_text(stmt.get(), 11);
            config.date_format = df ? df : "YYYY-MM-DD";
            
            config.return_home_delay = sqlite3_column_int(stmt.get(), 12);
            config.warning_record_count = sqlite3_column_int(stmt.get(), 13);

            // 【流程图节点K】读取周六/周日是否上班的规则开关
            config.sat_work = sqlite3_column_int(stmt.get(), 14);
            config.sun_work = sqlite3_column_int(stmt.get(), 15);
        }
    }

    return config;
}

int db_add_shift(const std::string& name, 
                 const std::string& s1_start, const std::string& s1_end,
                 const std::string& s2_start, const std::string& s2_end,
                 const std::string& s3_start, const std::string& s3_end,
                 int cross_day) {
                   
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = 
        "INSERT INTO shifts (name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    ScopedSqliteStmt stmt;
    int new_id = -1;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_STATIC);
        
        // 辅助 lambda：空字符串存 NULL 或 空串? 这里存空串即可
        auto bind_str = [&](int idx, const std::string& s) {
            sqlite3_bind_text(stmt.get(), idx, s.c_str(), -1, SQLITE_STATIC);
        };
        
        bind_str(2, s1_start); bind_str(3, s1_end);
        bind_str(4, s2_start); bind_str(5, s2_end);
        bind_str(6, s3_start); bind_str(7, s3_end);
        
        sqlite3_bind_int(stmt.get(), 8, cross_day);
        
        if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
            new_id = (int)sqlite3_last_insert_rowid(db);
        }
    }

    return new_id;
}

bool db_delete_shift(int shift_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "DELETE FROM shifts WHERE id=?;";
    ScopedSqliteStmt stmt;

    // 1. 准备语句
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Delete Shift Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 2. 绑定参数 (shift_id 绑定到第一个 ?)
    sqlite3_bind_int(stmt.get(), 1, shift_id);
    
    // 3. 执行语句
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);
    
    if (!ok) {
        std::cerr << "[Data] Delete Shift Execution Failed: " << sqlite3_errmsg(db) << std::endl;
    }

    return ok;
}

bool db_update_global_rules(const RuleConfig& config) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    // 强制更新 id=1 的记录
    const char* sql = "UPDATE attendance_rules SET "
                  "company_name=?, late_threshold=?, early_leave_threshold=?, "
                  "device_id=?, volume=?, screensaver_time=?, max_admins=?, "
                  "relay_delay=?, wiegand_fmt=?, "
                  "duplicate_punch_limit=?, language=?, date_format=?, "
                  "return_home_delay=?, warning_record_count=? "
                  "WHERE id=1;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update Rules Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 绑定原有参数
    sqlite3_bind_text(stmt.get(), 1, config.company_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 2, config.late_threshold);
    sqlite3_bind_int(stmt.get(), 3, config.early_leave_threshold);

    //  绑定新参数
    sqlite3_bind_text(stmt.get(), 1, config.company_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 2, config.late_threshold);
    sqlite3_bind_int(stmt.get(), 3, config.early_leave_threshold);
    sqlite3_bind_int(stmt.get(), 4, config.device_id);
    sqlite3_bind_int(stmt.get(), 5, config.volume);
    sqlite3_bind_int(stmt.get(), 6, config.screensaver_time);
    sqlite3_bind_int(stmt.get(), 7, config.max_admins);
    sqlite3_bind_int(stmt.get(), 8, config.relay_delay);
    sqlite3_bind_int(stmt.get(), 9, config.wiegand_fmt);
    sqlite3_bind_int(stmt.get(), 10, config.duplicate_punch_limit);
    sqlite3_bind_text(stmt.get(), 11, config.language.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 12, config.date_format.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 13, config.return_home_delay);
    sqlite3_bind_int(stmt.get(), 14, config.warning_record_count);
    
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    if(ok) {
        std::cout << "[Data] System Config Updated." << std::endl;
    }
    return ok;
}

// ================= 3. 用户管理 DAO  =================

int db_add_user(const UserData& user, const cv::Mat& face_image) {
    // 1. 无需锁的耗时操作：保存注册时人脸照片到磁盘
    std::string path_str = "";
    if (!face_image.empty()) {
        // 确保注册时人脸照片文件夹存在
        if (!fs::exists(AVATAR_DIR)) fs::create_directories(AVATAR_DIR);

        // 用时间戳或随机数+名字作为文件名
        long long now = std::time(nullptr);
        std::string fname = std::to_string(now) + "_" + user.name + ".jpg";
        fs::path p = fs::path(AVATAR_DIR) / fname;
        
        try {
            if (cv::imwrite(p.string(), face_image)) {
                path_str = p.string();
            }
        } catch (...) {
            std::cerr << "[Data] Save Avatar Image Failed." << std::endl;
        }
    }

    // 2. 核心数据库写入：精确加锁
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 注意：这里的 SQL 加入了 avatar_path
    const char* sql = 
        "INSERT INTO users (name, password, card_id, privilege, avatar_path, dept_id) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    
    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt.get(), 1, user.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, user.password.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, user.card_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 4, user.role);

        // 存路径
        if (!path_str.empty()) {
            sqlite3_bind_text(stmt.get(), 5, path_str.c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt.get(), 5);
        }

        if (user.dept_id > 0) {
            sqlite3_bind_int(stmt.get(), 6, user.dept_id);
        } else {
            sqlite3_bind_null(stmt.get(), 6);
        }

        if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
            return sqlite3_last_insert_rowid(db);
        }
    }
    
    return -1;
}

//批量导入/同步员工数据 (用于 U盘/网络批量同步)
bool db_batch_add_users(const std::vector<UserData>& users_list) {
    if (users_list.empty()) return true;

    // 1. 获取排他锁，防止批量写入时与其他线程的读写操作冲突
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 2. 开启事务 (BEGIN TRANSACTION)，极大地加速批量插入
    char* zErrMsg = 0;
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &zErrMsg) != SQLITE_OK) {
        std::cerr << "[Data] Begin Transaction Failed: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }

    // 3. 准备 SQL 语句 
    // 使用 INSERT OR REPLACE：如果 id 已存在，则覆盖更新；如果不存在，则新增。
    // 这里包含你 sql_users 表中除了自增ID外的所有关键业务字段
    const char* sql = "INSERT OR REPLACE INTO users "
                      "(id, name, password, card_id, privilege, face_data, avatar_path, fingerprint_data, dept_id, default_shift_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Batch Add Users Failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0); // 发生错误，回滚
        return false;
    }

    bool success = true;

    // 4. 循环绑定数据并执行
    for (const auto& user : users_list) {
        // 绑定 1: id (工号)
        sqlite3_bind_int(stmt.get(), 1, user.id);
        
        // 绑定 2: name (姓名)
        sqlite3_bind_text(stmt.get(), 2, user.name.c_str(), -1, SQLITE_STATIC);
        
        // 绑定 3: password (登录密码)
        if (user.password.empty()) sqlite3_bind_null(stmt.get(), 3);
        else sqlite3_bind_text(stmt.get(), 3, user.password.c_str(), -1, SQLITE_STATIC);
        
        // 绑定 4: card_id (IC/ID卡号)
        if (user.card_id.empty()) sqlite3_bind_null(stmt.get(), 4);
        else sqlite3_bind_text(stmt.get(), 4, user.card_id.c_str(), -1, SQLITE_STATIC);
        
        // 绑定 5: privilege (对应 UserData 里的 role)
        sqlite3_bind_int(stmt.get(), 5, user.role);

        // 绑定 6: face_data (对应 UserData 里的 face_feature 二进制流)
        if (user.face_feature.empty()) {
            sqlite3_bind_null(stmt.get(), 6);
        } else {
            sqlite3_bind_blob(stmt.get(), 6, user.face_feature.data(), user.face_feature.size(), SQLITE_STATIC);
        }

        // 绑定 7: avatar_path (人脸图片路径)
        if (user.avatar_path.empty()) sqlite3_bind_null(stmt.get(), 7);
        else sqlite3_bind_text(stmt.get(), 7, user.avatar_path.c_str(), -1, SQLITE_STATIC);

        // 绑定 8: fingerprint_data (对应 UserData 里的 fingerprint_feature 二进制流)
        if (user.fingerprint_feature.empty()) {
            sqlite3_bind_null(stmt.get(), 8);
        } else {
            sqlite3_bind_blob(stmt.get(), 8, user.fingerprint_feature.data(), user.fingerprint_feature.size(), SQLITE_STATIC);
        }

        // 绑定 9: dept_id (部门ID)
        if (user.dept_id <= 0) sqlite3_bind_null(stmt.get(), 9); 
        else sqlite3_bind_int(stmt.get(), 9, user.dept_id);

        // 绑定 10: default_shift_id (默认班次ID)
        if (user.default_shift_id <= 0) sqlite3_bind_null(stmt.get(), 10);
        else sqlite3_bind_int(stmt.get(), 10, user.default_shift_id);

        // 执行单条语句
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            std::cerr << "[Data] Batch Insert Error on User ID " << user.id 
                      << ": " << sqlite3_errmsg(db) << std::endl;
            success = false;
            break; // 出现错误，跳出循环
        }

        // 重置语句状态，以便下一次循环可以重新绑定新数据！
        sqlite3_clear_bindings(stmt.get());
        sqlite3_reset(stmt.get());
    }

    // 5. 根据执行结果提交或回滚
    if (success) {
        sqlite3_exec(db, "COMMIT;", 0, 0, 0); // 正式写入磁盘
        std::cout << "[Data] Successfully batch synced " << users_list.size() << " users." << std::endl;
    } else {
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0); // 放弃之前的所有更改
        std::cerr << "[Data] Batch sync failed, all changes rolled back." << std::endl;
    }

    return success;
}

std::optional<UserData> db_get_user_info(int user_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    const char* sql = 
        "SELECT u.id, u.name, u.password, u.card_id, u.privilege, u.dept_id, "
        "u.face_data, u.fingerprint_data, d.name, u.avatar_path "
        "FROM users u "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE u.id=?;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt.get(), 1, user_id);
        
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            UserData u; // 将对象定义移到这里，查到了才创建
            
            // [0] ID
            u.id = sqlite3_column_int(stmt.get(), 0);
            
            // [1] Name
            const char* name = (const char*)sqlite3_column_text(stmt.get(), 1);
            u.name = name ? name : "";
            
            // [2] Password
            const char* pwd = (const char*)sqlite3_column_text(stmt.get(), 2);
            u.password = pwd ? pwd : "";
            
            // [3] Card ID
            const char* card = (const char*)sqlite3_column_text(stmt.get(), 3);
            u.card_id = card ? card : "";
            
            // [4] Role (DB字段是 role)
            u.role = sqlite3_column_int(stmt.get(), 4);
            
            // [5] Dept ID
            u.dept_id = sqlite3_column_int(stmt.get(), 5);
            
            // [6] Face Data (人脸数据)
            const void* face_blob = sqlite3_column_blob(stmt.get(), 6);
            int face_bytes = sqlite3_column_bytes(stmt.get(), 6);
            if (face_blob && face_bytes > 0) {
                const uint8_t* ptr = (const uint8_t*)face_blob;
                u.face_feature.assign(ptr, ptr + face_bytes);
            }

            // [7] Fingerprint Data (指纹数据)
            const void* fp_blob = sqlite3_column_blob(stmt.get(), 7);
            int fp_bytes = sqlite3_column_bytes(stmt.get(), 7);
            if (fp_blob && fp_bytes > 0) {
                const uint8_t* ptr = (const uint8_t*)fp_blob;
                u.fingerprint_feature.assign(ptr, ptr + fp_bytes);
            }

            // [8] Dept Name (部门名称) - 修复部门不显示的问题
            const char* dname = (const char*)sqlite3_column_text(stmt.get(), 8);
            u.dept_name = dname ? dname : "Unknown"; // 如果没部门，显示Unknown

            // [9] Avatar Path (注册时人脸照片存储路径)
            const char* avatar = (const char*)sqlite3_column_text(stmt.get(), 9);
            u.avatar_path = avatar ? avatar : ""; // 如果数据库里存了路径就取出来，没有就是空字符串

            return u; // 查到了数据，直接返回，C++会自动把它装进 optional 的“盒子”里
        }
    } else {
        std::cerr << "[Data] Get User Info SQL Error: " << sqlite3_errmsg(db) << std::endl;
    }
    
    // 走到这里说明要么 prepare 失败，要么没查到(step 不是 ROW)
    return std::nullopt; 
}

bool db_delete_user(int user_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "DELETE FROM users WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt.get(), 1, user_id);
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);
    
    return ok;
}

std::vector<UserData> db_get_all_users() {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<UserData> users;
    const char* sql = "SELECT u.id, u.name, u.dept_id, u.privilege, u.password, u.card_id, u.face_data, d.name, u.avatar_path "
                  "FROM users u "
                  "LEFT JOIN departments d ON u.dept_id = d.id";
                      
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        printf("[DB] Prepare error: %s\n", sqlite3_errmsg(db));
        return users;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        UserData u;
        u.id = sqlite3_column_int(stmt.get(), 0);
        u.name = (const char*)sqlite3_column_text(stmt.get(), 1);
        u.dept_id = sqlite3_column_int(stmt.get(), 2);
        u.role = sqlite3_column_int(stmt.get(), 3);
        
        const char* pwd = (const char*)sqlite3_column_text(stmt.get(), 4);
        u.password = pwd ? pwd : "";
        
        const char* card = (const char*)sqlite3_column_text(stmt.get(), 5);
        u.card_id = card ? card : "";

        const void* blob = sqlite3_column_blob(stmt.get(), 6);
        int bytes = sqlite3_column_bytes(stmt.get(), 6);
        if (bytes > 0 && blob) {
             const uint8_t* ptr = (const uint8_t*)blob;
             u.face_feature.assign(ptr, ptr + bytes);
        }

        // 获取关联查询出来的部门名称 (第7列)
        const char* d_name = (const char*)sqlite3_column_text(stmt.get(), 7);
        u.dept_name = d_name ? d_name : "Unknown"; 

        const char* avatar = (const char*)sqlite3_column_text(stmt.get(), 8);
        u.avatar_path = avatar ? avatar : ""; 

        users.push_back(u);
    }

    return users;
}

bool db_assign_user_shift(int user_id, int shift_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "UPDATE users SET default_shift_id=? WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;
    
    if (shift_id > 0) sqlite3_bind_int(stmt.get(), 1, shift_id);
    else sqlite3_bind_null(stmt.get(), 1); // 解除排班
    
    sqlite3_bind_int(stmt.get(), 2, user_id);
    
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

ShiftInfo db_get_user_shift(int user_id) {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    ShiftInfo s = {0, "", "", "", "", "", "", "", 0}; // 初始化空结构
    
    const char* sql = 
        "SELECT s.id, s.name, "
        "s.s1_start, s.s1_end, s.s2_start, s.s2_end, s.s3_start, s.s3_end, s.cross_day "
        "FROM users u "
        "JOIN shifts s ON u.default_shift_id = s.id "
        "WHERE u.id = ?;";
        
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt.get(), 1, user_id);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            s.id = sqlite3_column_int(stmt.get(), 0);
            s.name = (const char*)sqlite3_column_text(stmt.get(), 1);
            
            // 提取所有时段
            auto get_col = [&](int i){ const char* t = (const char*)sqlite3_column_text(stmt.get(), i); return t?t:""; };
            s.s1_start = get_col(2); s.s1_end = get_col(3);
            s.s2_start = get_col(4); s.s2_end = get_col(5);
            s.s3_start = get_col(6); s.s3_end = get_col(7);
            
            s.cross_day = sqlite3_column_int(stmt.get(), 8);
        }
    }

    return s;
}

//  用户信息修改接口 (不含人脸，密码)
bool db_update_user_basic(int user_id, const std::string& name, int dept_id, int privilege, const std::string& card_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = 
        "UPDATE users SET name=?, dept_id=?, privilege=?, card_id=? WHERE id=?;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update User Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 绑定参数
    sqlite3_bind_text(stmt.get(), 1, name.c_str(), -1, SQLITE_STATIC);
    
    if (dept_id > 0) sqlite3_bind_int(stmt.get(), 2, dept_id);
    else sqlite3_bind_null(stmt.get(), 2); // 部门为空
    
    sqlite3_bind_int(stmt.get(), 3, privilege);
    sqlite3_bind_text(stmt.get(), 4, card_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 5, user_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);
    
    if (ok) std::cout << "[Data] User " << user_id << " info updated." << std::endl;
    return ok;
}

//单独更新用户人脸特征数据并更新存储路径(带旧图像清理)
bool db_update_user_face(int user_id, const cv::Mat& face_image) {
    
    if (face_image.empty()) {
        std::cerr << "[DB] Error: Cannot update face with empty image." << std::endl;
        return false;
    }

    // 1.查找并删除旧的人脸照片
    auto user_opt = db_get_user_info(user_id);
    if (user_opt.has_value()) {
        std::string old_path = user_opt.value().avatar_path;
        // 如果旧路径不为空，且文件真的存在硬盘上
        if (!old_path.empty() && fs::exists(old_path)) {
            try {
                fs::remove(old_path); // 删除旧文件
                std::cout << "[DB] Deleted old avatar: " << old_path << std::endl;
            } catch (const fs::filesystem_error& e) {
                std::cerr << "[DB] Warning: Failed to delete old avatar: " << e.what() << std::endl;
            }
        }
    }

    // 开始安全的写操作：加上写锁 (独占锁)
    std::unique_lock<std::shared_mutex> lock(g_db_mutex); 
    if (!db) return false;

    // 2. 保存新图片到本地文件夹
    if (!fs::exists(AVATAR_DIR)) {
        fs::create_directories(AVATAR_DIR);
    }
    
    std::string fname = std::to_string(std::time(nullptr)) + "_" + std::to_string(user_id) + ".jpg";
    fs::path p = fs::path(AVATAR_DIR) / fname;
    std::string path_str = p.string();

    try {
        if (!cv::imwrite(path_str, face_image)) {
            std::cerr << "[DB] Error: Failed to save new face image to disk." << std::endl;
            return false;
        }
    } catch (...) {
        std::cerr << "[DB] Error: Exception occurred while saving image." << std::endl;
        return false;
    }

    // 3. 更新数据库中的 avatar_path
    const char* sql = "UPDATE users SET avatar_path=? WHERE id=?;";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[DB] SQL Error: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, user_id);

    bool success = (sqlite3_step(stmt.get()) == SQLITE_DONE);
    
    if (success) {
        std::cout << "[DB] Update Face Avatar Path Success: " << path_str << std::endl;
    }
    
    return success;
}

// 用户密码更新接口
bool db_update_user_password(int user_id, const std::string& new_raw_password) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "UPDATE users SET password=? WHERE id=?;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    // 1. 对新密码进行哈希处理 (复用现有的哈希函数)
    std::string hashed_pwd = db_hash_password(new_raw_password);

    // 2. 绑定参数
    sqlite3_bind_text(stmt.get(), 1, hashed_pwd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, user_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);
    
    if (ok) std::cout << "[Data] User " << user_id << " password updated." << std::endl;
    return ok;
}

//单独修改/录入用户指纹特征
bool db_update_user_fingerprint(int user_id, const std::vector<uint8_t>& fingerprint_data) {
    // 写入操作，需要获取排他锁(写锁)
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 准备 SQL 更新语句
    const char* sql = "UPDATE users SET fingerprint_data = ? WHERE id = ?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update Fingerprint Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 1. 绑定第 1 个参数：fingerprint_data (BLOB类型)
    if (fingerprint_data.empty()) {
        // 如果传入空数组，意味着清空/删除该员工的指纹
        sqlite3_bind_null(stmt.get(), 1);
    } else {
        // 绑定二进制数据：使用 sqlite3_bind_blob
        // 参数解释: (语句句柄, 占位符索引, 数据指针, 数据大小, 内存管理策略)
        sqlite3_bind_blob(stmt.get(), 1, fingerprint_data.data(), fingerprint_data.size(), SQLITE_STATIC);
    }

    // 2. 绑定第 2 个参数：user_id (INTEGER类型)
    sqlite3_bind_int(stmt.get(), 2, user_id);

    // 执行 SQL 语句
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    if (ok) {
        // 检查是否有行被真正修改（防止传入了不存在的 user_id）
        if (sqlite3_changes(db) > 0) {
            std::cout << "[Data] Fingerprint updated successfully for user_id: " << user_id << std::endl;
        } else {
            std::cerr << "[Data] Warning: Fingerprint update affected 0 rows (user_id " << user_id << " might not exist)." << std::endl;
            ok = false;
        }
    } else {
        std::cerr << "[Data] Failed to update fingerprint for user_id: " << user_id 
                  << " Error: " << sqlite3_errmsg(db) << std::endl;
    }

    return ok;
}

// 轻量级用户列表加载 (仅 ID 和 Name)
std::vector<UserData> db_get_all_users_light() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁
    std::vector<UserData> users;
    
    // SQL 仅查询 id 和 name，绝对不查 face_data (BLOB)
    const char* sql = "SELECT id, name FROM users;"; 
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            UserData u;
            u.id = sqlite3_column_int(stmt.get(), 0);
            
            const char* txt = (const char*)sqlite3_column_text(stmt.get(), 1);
            u.name = txt ? txt : "Unknown";
            
            // 其他字段留空即可，因为只是为了映射名字
            users.push_back(u);
        }
    } else {
        std::cerr << "[Data] Light Load Failed: " << sqlite3_errmsg(db) << std::endl;
    }
    
    // 打印日志方便调试启动速度
    std::cout << "[Data] Light-loaded " << users.size() << " users (ID/Name only)." << std::endl;
    return users;
}

// ================= 4. 考勤记录 DAO =================

bool db_log_attendance(int user_id, int shift_id, const cv::Mat& image, int status) {
    long long now = std::time(nullptr);
    std::string path_str = "";
    
    // 1. 无需数据库的操作 (保存图片到磁盘) —— 不加锁，不阻塞别人！
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

    // 2. 纯粹的数据库插入操作 —— 用大括号包围，精确加锁！
    bool ok = false;
    {
        std::unique_lock<std::shared_mutex> lock(g_db_mutex);// 开始排他锁

        if (!g_stmt_log_attendance) {
            std::cerr << "[Data] Error: log_attendance statement is not precompiled!" << std::endl;
            return false;
        }

        sqlite3_reset(g_stmt_log_attendance);
        sqlite3_clear_bindings(g_stmt_log_attendance);

        sqlite3_bind_int(g_stmt_log_attendance, 1, user_id);
        
        if(shift_id > 0) 
            sqlite3_bind_int(g_stmt_log_attendance, 2, shift_id); 
        else 
            sqlite3_bind_null(g_stmt_log_attendance, 2);
            
        sqlite3_bind_text(g_stmt_log_attendance, 3, path_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(g_stmt_log_attendance, 4, now);
        sqlite3_bind_int(g_stmt_log_attendance, 5, status);

        ok = (sqlite3_step(g_stmt_log_attendance) == SQLITE_DONE);
    } // 离开大括号，排他锁瞬间释放！
    
    if(ok) {
        std::cout << "[Data] Attendance Logged -> User: " << user_id 
                  << " Time: " << now << " Status: " << static_cast<int>(status) << std::endl;
    } else {
        std::cerr << "[Data] Attendance Logged Failed: " << sqlite3_errmsg(db) << std::endl;
    }
    
    return ok;
}

// 获取最后打卡时间
time_t db_getLastPunchTime(int user_id) {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    if (!db) return 0;

    ScopedSqliteStmt stmt;
    // 查询该用户最新的打卡记录时间
    const char* sql = "SELECT timestamp FROM attendance WHERE user_id = ? ORDER BY timestamp DESC LIMIT 1;";
    time_t last_ts = 0;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt.get(), 1, user_id);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            last_ts = (time_t)sqlite3_column_int64(stmt.get(), 0);
        }
    }

    return last_ts;
}

//磁盘空间管理：自动清理
int db_cleanup_old_attendance_images(int days_old) {
    // 1. 计算时间阈值：当前时间减去 days_old 天的秒数
    time_t threshold = std::time(nullptr) - (days_old * 24 * 3600);
    
    // 独占锁，因为接下来我们要修改数据库
    std::unique_lock<std::shared_mutex> lock(g_db_mutex); 
    
    // 2. 查出所有超过时间阈值，且带有抓拍图片的考勤记录
    const char* select_sql = 
        "SELECT id, image_path FROM attendance_records "
        "WHERE timestamp < ? AND image_path IS NOT NULL AND image_path != '';";
        
    ScopedSqliteStmt select_stmt;
    if (sqlite3_prepare_v2(db, select_sql, -1, select_stmt.ptr(), nullptr) != SQLITE_OK) {
        std::cerr << "[DB Error] 清理查询失败: " << sqlite3_errmsg(db) << std::endl;
        return 0;
    }
    sqlite3_bind_int64(select_stmt.get(), 1, threshold);

    std::vector<int> ids_to_update;
    std::vector<std::string> files_to_delete;

    // 收集需要处理的数据
    while (sqlite3_step(select_stmt.get()) == SQLITE_ROW) {
        ids_to_update.push_back(sqlite3_column_int(select_stmt.get(), 0));
        const char* path = (const char*)sqlite3_column_text(select_stmt.get(), 1);
        if (path) files_to_delete.push_back(path);
    }

    if (ids_to_update.empty()) {
        return 0; // 没有需要清理的数据
    }

    // 3. 删除本地物理文件 (使用 C++17 filesystem)
    int deleted_count = 0;
    for (const auto& file_path : files_to_delete) {
        try {
            // 安全起见：只删除 IMAGE_DIR 目录下的文件，防止误删系统文件
            if (file_path.find(IMAGE_DIR) != std::string::npos && fs::exists(file_path)) {
                fs::remove(file_path);
                deleted_count++;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[Warning] 删除过时图片失败: " << e.what() << std::endl;
        }
    }

    // 4. 批量更新数据库，将这些记录的 image_path 置空
    // 考勤流水本身决不能删，只清空图片路径！
    const char* update_sql = "UPDATE attendance_records SET image_path = NULL WHERE id = ?;";
    ScopedSqliteStmt update_stmt;
    if (sqlite3_prepare_v2(db, update_sql, -1, update_stmt.ptr(), nullptr) == SQLITE_OK) {
        // 开启事务，加速批量更新
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        for (int id : ids_to_update) {
            sqlite3_bind_int(update_stmt.get(), 1, id);
            sqlite3_step(update_stmt.get());
            sqlite3_reset(update_stmt.get());
        }
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    }

    return deleted_count;
}


std::vector<AttendanceRecord> db_get_records(long long start_ts, long long end_ts) {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<AttendanceRecord> list;
    
    // 关联查询：attendance -> users -> departments
    const char* sql = 
        "SELECT a.id, a.user_id, a.timestamp, a.status, a.image_path, u.name, d.name "
        "FROM attendance a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE a.timestamp BETWEEN ? AND ? "
        "ORDER BY a.timestamp DESC;";
        
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int64(stmt.get(), 1, start_ts);
        sqlite3_bind_int64(stmt.get(), 2, end_ts);
        
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            AttendanceRecord r;
            r.id = sqlite3_column_int(stmt.get(), 0);
            r.user_id = sqlite3_column_int(stmt.get(), 1);
            r.timestamp = sqlite3_column_int64(stmt.get(), 2);
            r.status = sqlite3_column_int(stmt.get(), 3);
            
            const char* p = (const char*)sqlite3_column_text(stmt.get(), 4);
            r.image_path = p ? p : "";
            
            const char* uname = (const char*)sqlite3_column_text(stmt.get(), 5);
            r.user_name = uname ? uname : "Unknown";
            
            const char* dname = (const char*)sqlite3_column_text(stmt.get(), 6);
            r.dept_name = dname ? dname : "No Dept";
            
            list.push_back(r);
        }
    }

    return list;
}

//按工号和时间段查询个人的考勤记录
std::vector<AttendanceRecord> db_get_records_by_user(int user_id, long long start_ts, long long end_ts) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁
    std::vector<AttendanceRecord> records;

    // 联表查询
    const char* sql = 
        "SELECT a.id, a.user_id, u.name, d.name, a.timestamp, a.status, a.image_path "
        "FROM attendance a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE a.user_id = ? AND a.timestamp >= ? AND a.timestamp <= ? "
        "ORDER BY a.timestamp ASC;";

    ScopedSqliteStmt stmt; 
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get Records By User Failed: " << sqlite3_errmsg(db) << std::endl;
        return records;
    }

    // 绑定三个参数
    sqlite3_bind_int(stmt.get(), 1, user_id);
    sqlite3_bind_int64(stmt.get(), 2, start_ts);
    sqlite3_bind_int64(stmt.get(), 3, end_ts);

    // 循环提取每一行记录
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        AttendanceRecord rec;
        
        rec.id = sqlite3_column_int(stmt.get(), 0);
        rec.user_id = sqlite3_column_int(stmt.get(), 1);
        
        // 提取姓名，防空指针处理
        const char* u_name = (const char*)sqlite3_column_text(stmt.get(), 2);
        rec.user_name = u_name ? u_name : "";
        
        // 提取部门名称，防空指针处理
        const char* d_name = (const char*)sqlite3_column_text(stmt.get(), 3);
        rec.dept_name = d_name ? d_name : "";
        
        rec.timestamp = sqlite3_column_int64(stmt.get(), 4);
        rec.status = sqlite3_column_int(stmt.get(), 5);
        
        // 提取抓拍图片路径
        const char* img = (const char*)sqlite3_column_text(stmt.get(), 6);
        rec.image_path = img ? img : "";
        rec.minutes_late = 0;
        rec.minutes_early = 0;
        
        records.push_back(rec);
    }

    return records;
}

// ================= 5.数据库事务接口 =================

bool db_begin_transaction() {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）
    
    return exec_sql("BEGIN TRANSACTION;", "Tx Begin");
}

bool db_commit_transaction() {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

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
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁
    ShiftInfo s = {0, "", "", "", "", "", "", "", 0};
    if (shift_id <= 0) return s; // ID<=0 视为休息

    ScopedSqliteStmt stmt;

    const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day FROM shifts WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt.get(), 1, shift_id);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            s.id = sqlite3_column_int(stmt.get(), 0);
            s.name = (const char*)sqlite3_column_text(stmt.get(), 1);
            auto get_col = [&](int i){ const char* t = (const char*)sqlite3_column_text(stmt.get(), i); return t?t:""; };
            s.s1_start = get_col(2); s.s1_end = get_col(3);
            s.s2_start = get_col(4); s.s2_end = get_col(5);
            s.s3_start = get_col(6); s.s3_end = get_col(7);
            s.cross_day = sqlite3_column_int(stmt.get(), 8);
        }
    }

    return s;
}

bool db_set_dept_schedule(int dept_id, int day_of_week, int shift_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    // 使用 INSERT OR REPLACE (UPSERT)
    const char* sql = "INSERT OR REPLACE INTO dept_schedule (dept_id, day_of_week, shift_id) VALUES (?, ?, ?);";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt.get(), 1, dept_id);
    sqlite3_bind_int(stmt.get(), 2, day_of_week);
    sqlite3_bind_int(stmt.get(), 3, shift_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

bool db_set_user_special_schedule(int user_id, const std::string& date_str, int shift_id) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "INSERT OR REPLACE INTO user_schedule (user_id, date_str, shift_id) VALUES (?, ?, ?);";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt.get(), 1, user_id);
    sqlite3_bind_text(stmt.get(), 2, date_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 3, shift_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

// 智能排班查询
std::optional<ShiftInfo> db_get_user_shift_smart(int user_id, long long timestamp) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    int final_shift_id = 0;
    
    std::string date_str = timestamp_to_date(timestamp);
    int weekday = timestamp_to_weekday(timestamp);
    
    // 1. 优先级最高：检查个人特殊排班 (User Schedule)
    {
        const char* sql = "SELECT shift_id FROM user_schedule WHERE user_id=? AND date_str=?;";
        ScopedSqliteStmt stmt;
        if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt.get(), 1, user_id);
            sqlite3_bind_text(stmt.get(), 2, date_str.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
                final_shift_id = sqlite3_column_int(stmt.get(), 0);
                // ⚠️ 注意：这里千万不要手动 finalize，也不要直接 return，让他去下面查详情
            }
        }
    }

    // 2. 优先级第二：检查部门周排班 (如果第一步没找到)
    if (final_shift_id == 0) {
        int dept_id = 0;

        auto u_opt = db_get_user_info(user_id); // 安全获取
        if (u_opt.has_value()) {
            dept_id = u_opt->dept_id; // 如果用户存在，才去取他的 dept_id
        }
        
        if (dept_id > 0) {
            const char* sql = "SELECT shift_id FROM dept_schedule WHERE dept_id=? AND day_of_week=?;";
            ScopedSqliteStmt stmt;
            if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
                sqlite3_bind_int(stmt.get(), 1, dept_id);
                sqlite3_bind_int(stmt.get(), 2, weekday);
                if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
                    final_shift_id = sqlite3_column_int(stmt.get(), 0);
                }
            }
        }
    }

    // 3. 优先级最低：使用用户默认班次 (如果前两步都没找到)
    if (final_shift_id == 0) {
        const char* sql = "SELECT default_shift_id FROM users WHERE id=?;";
        ScopedSqliteStmt stmt;
        if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt.get(), 1, user_id);
            if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
                final_shift_id = sqlite3_column_int(stmt.get(), 0);
            }
        }
    }

    // 4. 判断经过三轮查找后，有没有排班？
    if (final_shift_id <= 0) {
        return std::nullopt; // 确实没排班，今天是休息日
    }

    // =====================================================================
    // 【流程图节点 K】读取星期六/星期日是否上班的规则
    // 对应流程图：无论通过个人、部门还是默认班次路径，进入考勤计算前都必须经过此节点
    // weekday: 0=周日, 6=周六
    // 注意：个人特殊排班（user_schedule）由管理员手动指定，已明确表达“当天要上班”的意图，
    // 因此节点K的周末开关只对部门排班和默认班次起效。
    // 如果是个人特殊排班，跳过周末检查（管理员明确安排了就尊重其意图）。
    // =====================================================================
    bool from_personal_special = false;
    {
        // 利用局部块重新查询一次，确认 final_shift_id 是否来自个人特殊排班
        const char* sql_chk = "SELECT COUNT(*) FROM user_schedule WHERE user_id=? AND date_str=?;";
        ScopedSqliteStmt stmt_chk;
        if (sqlite3_prepare_v2(db, sql_chk, -1, stmt_chk.ptr(), 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt_chk.get(), 1, user_id);
            sqlite3_bind_text(stmt_chk.get(), 2, date_str.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt_chk.get()) == SQLITE_ROW) {
                from_personal_special = (sqlite3_column_int(stmt_chk.get(), 0) > 0);
            }
        }
    }

    if (!from_personal_special) {
        // 不是个人特殊排班，则需过节点K的周末规则判断
        RuleConfig rules = db_get_global_rules();
        if (weekday == 6 && rules.sat_work == 0) {
            // 星期六且规则配置为不上班 -> 返回无排班
            return std::nullopt;
        }
        if (weekday == 0 && rules.sun_work == 0) {
            // 星期日且规则配置为不上班 -> 返回无排班
            return std::nullopt;
        }
    }

    // 5. 拿着查到的 final_shift_id，去 shifts 表获取真正的班次详细时间
    {
        const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day FROM shifts WHERE id=?;";
        ScopedSqliteStmt stmt;
        if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt.get(), 1, final_shift_id);
            if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
                ShiftInfo shift;
                shift.id = sqlite3_column_int(stmt.get(), 0);
                
                // 安全获取文本数据，防止 NULL 导致奔溃
                auto get_text = [&](int col) -> std::string {
                    const unsigned char* text = sqlite3_column_text(stmt.get(), col);
                    return text ? reinterpret_cast<const char*>(text) : "";
                };

                shift.name     = get_text(1);
                shift.s1_start = get_text(2);
                shift.s1_end   = get_text(3);
                shift.s2_start = get_text(4);
                shift.s2_end   = get_text(5);
                shift.s3_start = get_text(6);
                shift.s3_end   = get_text(7);
                shift.cross_day = sqlite3_column_int(stmt.get(), 8);

                return shift; // 完美返回装有详细数据的盒子！
            }
        }
    }

    // 如果在 shifts 表里硬是没查到这个班次（可能被删了），也当做无排班
    return std::nullopt;
}

// ================= 重新/删除数据  =================

/**
 * @brief 获取最后保存图像的ID
 * @return 最后保存图像的ID，失败返回 -1
 */
long long data_getLastImageID() {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    if (!db) {
        std::cerr << "[Data] Error: Database not initialized!" << std::endl;
        return -1;
    }// 校验数据库连接

    const char* sql = "SELECT id FROM attendance ORDER BY id DESC LIMIT 1;";// 获取最后一条记录的ID
    ScopedSqliteStmt stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0);// 预编译 SQL
    if (rc != SQLITE_OK) {
        std::cerr << "[Data] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }// 预编译失败

    rc = sqlite3_step(stmt.get());// 执行 SQL
    long long last_id = -1;// 默认返回值
    if (rc == SQLITE_ROW) {
        last_id = sqlite3_column_int64(stmt.get(), 0);
    }// 成功获取到数据 
    else {
        std::cerr << "[Data] No images found in database." << std::endl;
    }// 未找到数据

    return last_id;// 返回最后保存的ID
}

// =================  系统维护接口 =================

/**
 * @brief  清空所有考勤记录
 * @details 删除 attendance 表数据，清空 captured_images 目录
 */
bool db_clear_attendance() {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）
    
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

/**
 * @brief  清空所有员工数据
 * @details 删除 users 表数据及其关联的图片文件
 */
bool db_clear_users() {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

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

/**
 * @brief  恢复出厂设置
 * @details 清除所有数据库数据和图片，重置系统
 */
bool db_factory_reset() {
    std::cout << "[Data] !!! FACTORY RESET !!!" << std::endl;
    
    // 1. 关闭数据库（data_close 内部自带写锁，这里不需要我们在外层加锁）
    data_close(); 
    
    // 2. 独占锁区域：只保护文件系统的删除操作
    {
        std::unique_lock<std::shared_mutex> lock(g_db_mutex); // 排他锁（写锁）
        try {
            if (fs::exists(DB_NAME)) fs::remove(DB_NAME);
            if (fs::exists(IMAGE_DIR)) fs::remove_all(IMAGE_DIR);
        } catch (const std::exception& e) {
            std::cerr << "[Data] Factory Reset FS Error: " << e.what() << std::endl;
        }
    } // 离开大括号，写锁自动释放

    // 3. 重新初始化（data_init 内部的 exec_sql 会自己去拿写锁）
    return data_init();
}

// =================  铃声管理接口 =================

std::vector<BellSchedule> db_get_all_bells() {
    
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<BellSchedule> list;
    const char* sql = "SELECT id, time, duration, days_mask, enabled FROM bells ORDER BY id ASC;";
    
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            BellSchedule b;
            b.id = sqlite3_column_int(stmt.get(), 0);
            b.time = (const char*)sqlite3_column_text(stmt.get(), 1);
            b.duration = sqlite3_column_int(stmt.get(), 2);
            b.days_mask = sqlite3_column_int(stmt.get(), 3);
            b.enabled = sqlite3_column_int(stmt.get(), 4);
            list.push_back(b);
        }
    }

    return list;
}

// 更新铃声设置
bool db_update_bell(const BellSchedule& bell) {
    
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "UPDATE bells SET time=?, duration=?, days_mask=?, enabled=? WHERE id=?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt.get(), 1, bell.time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 2, bell.duration);
    sqlite3_bind_int(stmt.get(), 3, bell.days_mask);
    sqlite3_bind_int(stmt.get(), 4, bell.enabled ? 1 : 0);
    sqlite3_bind_int(stmt.get(), 5, bell.id);
    
    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

// =================  查询系统信息接口 =================

//查询系统信息（员工注册数，管理员注册数，人脸注册数，指纹注册数，卡号注册数）
SystemStats db_get_system_stats() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁
    SystemStats stats = {0, 0, 0, 0, 0};

    // 🌟 核心：利用聚合函数一次性查出所有结果，性能极高
    const char* sql = 
        "SELECT "
        "  COUNT(*), "                                                         // 0: 总人数
        "  SUM(CASE WHEN privilege = 1 THEN 1 ELSE 0 END), "                   // 1: 管理员数
        "  SUM(CASE WHEN face_data IS NOT NULL THEN 1 ELSE 0 END), "           // 2: 录入人脸数
        "  SUM(CASE WHEN fingerprint_data IS NOT NULL THEN 1 ELSE 0 END), "    // 3: 录入指纹数
        "  SUM(CASE WHEN card_id IS NOT NULL AND card_id != '' THEN 1 ELSE 0 END) " // 4: 录入卡号数
        "FROM users;";

    ScopedSqliteStmt stmt; // 智能管理类
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {

        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            stats.total_employees    = sqlite3_column_int(stmt.get(), 0);
            stats.total_admins       = sqlite3_column_int(stmt.get(), 1);
            stats.total_faces        = sqlite3_column_int(stmt.get(), 2);
            stats.total_fingerprints = sqlite3_column_int(stmt.get(), 3);
            stats.total_cards        = sqlite3_column_int(stmt.get(), 4);
        }
    }

    return stats;
}


// ================= 系统全局配置接口 =================

//获取系统全局配置值
std::string db_get_system_config(const std::string& key, const std::string& default_value) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁

    const char* sql = "SELECT config_value FROM system_config WHERE config_key = ?;";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get System Config Failed: " << sqlite3_errmsg(db) << std::endl;
        return default_value;
    }

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const char* val = (const char*)sqlite3_column_text(stmt.get(), 0);
        return val ? std::string(val) : default_value;
    }

    return default_value; // 没查到则返回默认值
}

//设置系统全局配置值 (存在则更新，不存在则插入)
bool db_set_system_config(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex); // 写操作使用排他锁

    // 使用 INSERT OR REPLACE，键存在就覆盖，不存在就新增
    const char* sql = "INSERT OR REPLACE INTO system_config (config_key, config_value) VALUES (?, ?);";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Set System Config Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, value.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::cerr << "[Data] Set System Config Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    return true;
}


// ================= 全局节假日管理接口 =================

//设置全局节假日 (新增或修改)
bool db_set_holiday(const std::string& date_str, const std::string& holiday_name) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex); // 写操作使用排他锁

    const char* sql = "INSERT OR REPLACE INTO holidays (date_str, name) VALUES (?, ?);";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Set Holiday Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, date_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, holiday_name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::cerr << "[Data] Set Holiday Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    return true;
}

//删除指定的全局节假日 (例如取消放假)
bool db_delete_holiday(const std::string& date_str) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex); // 写操作使用排他锁

    const char* sql = "DELETE FROM holidays WHERE date_str = ?;";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Delete Holiday Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, date_str.c_str(), -1, SQLITE_STATIC);

    return (sqlite3_step(stmt.get()) == SQLITE_DONE);
}

//检查某天是否为全局节假日
std::optional<std::string> db_get_holiday(const std::string& date_str) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁

    const char* sql = "SELECT name FROM holidays WHERE date_str = ?;";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get Holiday Failed: " << sqlite3_errmsg(db) << std::endl;
        return std::nullopt;
    }

    sqlite3_bind_text(stmt.get(), 1, date_str.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt.get(), 0);
        if (name) {
            return std::string(name);
        }
    }

    return std::nullopt; // 没查到，说明今天不是节假日
}


// ================= 考勤设置与排班管理接口 =================

//批量导入部门排班数据
int db_import_dept_schedules(const std::vector<DeptScheduleEntry>& schedules) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex); // 写操作使用排他锁
    
    if (schedules.empty()) {
        return 0;
    }
    
    // 开启事务
    if (!exec_sql("BEGIN TRANSACTION;", "Begin transaction for import schedules")) {
        return 0;
    }
    
    int success_count = 0;
    
    for (const auto& entry : schedules) {
        // 验证数据：shift_id为0表示节假日，不插入记录
        if (entry.shift_id == ScheduleConstants::HOLIDAY) {
            continue;
        }
        
        // 验证班次ID范围：1-10
        if (entry.shift_id < ScheduleConstants::MIN_SHIFT_ID ||
            entry.shift_id > ScheduleConstants::MAX_SHIFT_ID) {
            std::cerr << "[Data] Invalid shift_id: " << entry.shift_id
                      << " for dept_id: " << entry.dept_id
                      << ", day: " << entry.day_of_week << std::endl;
            continue;
        }
        
        // 使用INSERT OR REPLACE确保一个部门一天只有一条记录
        const char* sql = "INSERT OR REPLACE INTO dept_schedule (dept_id, day_of_week, shift_id) VALUES (?, ?, ?);";
        ScopedSqliteStmt stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
            std::cerr << "[Data] Prepare import schedule failed: " << sqlite3_errmsg(db) << std::endl;
            continue;
        }
        
        sqlite3_bind_int(stmt.get(), 1, entry.dept_id);
        sqlite3_bind_int(stmt.get(), 2, entry.day_of_week);
        sqlite3_bind_int(stmt.get(), 3, entry.shift_id);
        
        if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
            success_count++;
        } else {
            std::cerr << "[Data] Execute import schedule failed: " << sqlite3_errmsg(db) << std::endl;
        }
    }
    
    // 提交事务
    if (!exec_sql("COMMIT;", "Commit transaction for import schedules")) {
        // 回滚
        exec_sql("ROLLBACK;", "Rollback transaction for import schedules");
        return 0;
    }
    
    std::cout << "[Data] Imported " << success_count << " department schedule entries." << std::endl;
    return success_count;
}

//获取部门完整排班视图
DeptScheduleView db_get_dept_schedule_view(int dept_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁
    DeptScheduleView view;
    view.dept_id = dept_id;
    
    // 初始化数组为节假日（0）
    for (int i = 0; i < 7; i++) {
        view.shifts[i] = ScheduleConstants::HOLIDAY;
    }
    
    // 获取部门名称
    const char* sql_dept = "SELECT name FROM departments WHERE id = ?;";
    ScopedSqliteStmt stmt_dept;
    
    if (sqlite3_prepare_v2(db, sql_dept, -1, stmt_dept.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt_dept.get(), 1, dept_id);
        if (sqlite3_step(stmt_dept.get()) == SQLITE_ROW) {
            const char* name = (const char*)sqlite3_column_text(stmt_dept.get(), 0);
            if (name) {
                view.dept_name = name;
            }
        }
    }
    
    // 获取排班数据
    const char* sql = "SELECT day_of_week, shift_id FROM dept_schedule WHERE dept_id = ? ORDER BY day_of_week;";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare get dept schedule failed: " << sqlite3_errmsg(db) << std::endl;
        return view;
    }
    
    sqlite3_bind_int(stmt.get(), 1, dept_id);
    
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int day_of_week = sqlite3_column_int(stmt.get(), 0);
        int shift_id = sqlite3_column_int(stmt.get(), 1);
        
        if (day_of_week >= 0 && day_of_week < 7) {
            view.shifts[day_of_week] = shift_id;
        }
    }
    
    return view;
}

//获取所有班次（限制最多10个）
std::vector<ShiftInfo> db_get_all_shifts_limited() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁
    std::vector<ShiftInfo> shifts;
    
    const char* sql = "SELECT id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day FROM shifts ORDER BY id LIMIT 10;";
    ScopedSqliteStmt stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare get all shifts limited failed: " << sqlite3_errmsg(db) << std::endl;
        return shifts;
    }
    
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        ShiftInfo shift;
        shift.id = sqlite3_column_int(stmt.get(), 0);
        
        const char* name = (const char*)sqlite3_column_text(stmt.get(), 1);
        shift.name = name ? name : "";
        
        const char* s1_start = (const char*)sqlite3_column_text(stmt.get(), 2);
        shift.s1_start = s1_start ? s1_start : "";
        
        const char* s1_end = (const char*)sqlite3_column_text(stmt.get(), 3);
        shift.s1_end = s1_end ? s1_end : "";
        
        const char* s2_start = (const char*)sqlite3_column_text(stmt.get(), 4);
        shift.s2_start = s2_start ? s2_start : "";
        
        const char* s2_end = (const char*)sqlite3_column_text(stmt.get(), 5);
        shift.s2_end = s2_end ? s2_end : "";
        
        const char* s3_start = (const char*)sqlite3_column_text(stmt.get(), 6);
        shift.s3_start = s3_start ? s3_start : "";
        
        const char* s3_end = (const char*)sqlite3_column_text(stmt.get(), 7);
        shift.s3_end = s3_end ? s3_end : "";
        
        shift.cross_day = sqlite3_column_int(stmt.get(), 8);
        
        shifts.push_back(shift);
    }
    
    return shifts;
}

// ================= 报表辅助批量查询接口 =================

//根据时间段批量获取全公司的打卡记录 (用于生成月度总表)
std::vector<AttendanceRecord> db_get_all_records_by_time(long long start_ts, long long end_ts) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁
    std::vector<AttendanceRecord> records;

    // 三表联查，一次性把所有报表需要的“人名”、“部门名”、“打卡信息”全捞出来
    // 并且按照 部门 -> 用户ID -> 打卡时间 排序，方便业务层直接按顺序输出到 Excel
    const char* sql = 
        "SELECT a.id, a.user_id, u.name, d.name, a.timestamp, a.status, a.image_path "
        "FROM attendance a "
        "LEFT JOIN users u ON a.user_id = u.id "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE a.timestamp >= ? AND a.timestamp <= ? "
        "ORDER BY d.id ASC, a.user_id ASC, a.timestamp ASC;";

    ScopedSqliteStmt stmt; 
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get All Records Failed: " << sqlite3_errmsg(db) << std::endl;
        return records;
    }

    // 绑定时间参数
    sqlite3_bind_int64(stmt.get(), 1, start_ts);
    sqlite3_bind_int64(stmt.get(), 2, end_ts);

    // 循环提取每一行记录
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        AttendanceRecord rec;
        
        rec.id = sqlite3_column_int(stmt.get(), 0);
        rec.user_id = sqlite3_column_int(stmt.get(), 1);
        
        // 提取姓名，防空指针处理
        const char* u_name = (const char*)sqlite3_column_text(stmt.get(), 2);
        rec.user_name = u_name ? u_name : "";
        
        // 提取部门名称，防空指针处理
        const char* d_name = (const char*)sqlite3_column_text(stmt.get(), 3);
        rec.dept_name = d_name ? d_name : "";
        
        rec.timestamp = sqlite3_column_int64(stmt.get(), 4);
        
        rec.status = sqlite3_column_int(stmt.get(), 5);
        
        // 提取抓拍图片路径
        const char* img = (const char*)sqlite3_column_text(stmt.get(), 6);
        rec.image_path = img ? img : "";
        
        rec.minutes_late = 0;
        rec.minutes_early = 0;
        
        records.push_back(rec);
    }

    return records;
}

//获取某部门下的所有用户列表 (用于按部门导出报表)
std::vector<UserData> db_get_users_by_dept(int dept_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    std::vector<UserData> users;

    const char* sql = "SELECT id, name, privilege, dept_id, default_shift_id FROM users WHERE dept_id = ?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get Users By Dept Failed: " << sqlite3_errmsg(db) << std::endl;
        return users;
    }

    sqlite3_bind_int(stmt.get(), 1, dept_id);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        UserData u;
        u.id = sqlite3_column_int(stmt.get(), 0);
        
        const char* name = (const char*)sqlite3_column_text(stmt.get(), 1);
        u.name = name ? name : "";
        
        u.role = sqlite3_column_int(stmt.get(), 2);
        u.dept_id = sqlite3_column_int(stmt.get(), 3);
        u.default_shift_id = sqlite3_column_int(stmt.get(), 4);
        
        users.push_back(u);
    }

    return users;
}
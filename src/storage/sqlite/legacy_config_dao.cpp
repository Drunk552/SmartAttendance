/**
 * @file
 * @brief 承接旧公司、配置、节假日和维护 SQLite DAO。
 */

#include "data/db_storage.h"
#include "storage/sqlite/legacy_db_internal.h"

#include <sqlite3.h>
#include <opencv2/imgcodecs.hpp>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ================= 7. 公司管理接口 (Company DAO) =================

/**
 * @brief 获取默认公司ID（通常是第一个公司）
 * @return 默认公司ID，如果没有公司返回1
 */
int db_get_default_company_id() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "SELECT id FROM companies ORDER BY id LIMIT 1";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            return sqlite3_column_int(stmt.get(), 0);
        }
    }
    return 1; // 默认返回1
}

/**
 * @brief 保存公司名称到数据库
 * @param name 公司名称
 * @return true 保存成功；false 保存失败
 */
bool db_save_company_name(const std::string& name) {
    // 更新默认公司（ID=1）的名称
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql =
        "INSERT OR REPLACE INTO companies (id, name, updated_at) "
        "VALUES (1, ?, datetime('now'))";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }
    return false;
}

//从数据库加载公司名称
bool db_load_company_name(std::string& name) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "SELECT name FROM companies WHERE id = 1";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* db_name = (const char*)sqlite3_column_text(stmt, 0);
            name = db_name ? db_name : "";
            sqlite3_finalize(stmt);
            return true;
        }
        sqlite3_finalize(stmt);
    }
    name = "未设置";
    return true;
}

//添加新公司
int db_add_company(const CompanyInfo& company) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql =
        "INSERT INTO companies (name, code, address, contact_phone, contact_email) "
        "VALUES (?, ?, ?, ?, ?)";

    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt.get(), 1, company.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, company.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 3, company.address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 4, company.contact_phone.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 5, company.contact_email.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        return -1;
    }

    return sqlite3_last_insert_rowid(db);
}

//获取所有公司列表
std::vector<CompanyInfo> db_get_all_companies() {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    std::vector<CompanyInfo> companies;

    const char* sql = "SELECT id, name, code, address, contact_phone, contact_email, created_at, updated_at FROM companies ORDER BY name";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            CompanyInfo company;
            company.id = sqlite3_column_int(stmt.get(), 0);

            auto get_text = [&](int col) -> std::string {
                const char* txt = (const char*)sqlite3_column_text(stmt.get(), col);
                return txt ? txt : "";
            };

            company.name = get_text(1);
            company.code = get_text(2);
            company.address = get_text(3);
            company.contact_phone = get_text(4);
            company.contact_email = get_text(5);
            company.created_at = get_text(6);
            company.updated_at = get_text(7);

            companies.push_back(company);
        }
    }

    return companies;
}

//获取公司信息
std::optional<CompanyInfo> db_get_company_info(int company_id) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "SELECT id, name, code, address, contact_phone, contact_email, created_at, updated_at FROM companies WHERE id = ?";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int(stmt.get(), 1, company_id);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    CompanyInfo company;
    company.id = sqlite3_column_int(stmt.get(), 0);

    auto get_text = [&](int col) -> std::string {
        const char* txt = (const char*)sqlite3_column_text(stmt.get(), col);
        return txt ? txt : "";
    };

    company.name = get_text(1);
    company.code = get_text(2);
    company.address = get_text(3);
    company.contact_phone = get_text(4);
    company.contact_email = get_text(5);
    company.created_at = get_text(6);
    company.updated_at = get_text(7);

    return company;
}

//更新公司信息
bool db_update_company(const CompanyInfo& company) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql =
        "UPDATE companies SET name = ?, code = ?, address = ?, contact_phone = ?, contact_email = ?, updated_at = datetime('now') "
        "WHERE id = ?";

    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, company.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, company.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 3, company.address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 4, company.contact_phone.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 5, company.contact_email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 6, company.id);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

//删除公司
bool db_delete_company(int company_id) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "DELETE FROM companies WHERE id = ?";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, company_id);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

//更新部门所属公司
bool db_update_department_company(int dept_id, int company_id) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "UPDATE departments SET company_id = ? WHERE id = ?";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, company_id);
    sqlite3_bind_int(stmt.get(), 2, dept_id);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
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
 * @brief 清空所有员工考勤记录和员工数据
 * @details 将删除 attendance、user_schedule、users 表中的数据，并清空对应的图片存储目录
 * @param keep_admin 是否保留管理员账号 (默认 true，防止清空后无法登录系统)
 * @return 成功返回 true，失败返回 false
 */
bool db_clear_all_employee_data(bool keep_admin) {
    // 1. 获取排他锁（写锁），保证操作期间其他线程无法读写数据库
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    // 2. 开启事务，保证删除操作的原子性
    exec_sql("BEGIN TRANSACTION;", "Begin Transaction");

    bool success = true;

    // 清空考勤记录表
    if (!exec_sql("DELETE FROM attendance;", "Clear Attendance")) success = false;

    // 清空用户排班表
    if (!exec_sql("DELETE FROM user_schedule;", "Clear User Schedule")) success = false;

    // 清空员工表 (根据参数决定是否保留管理员 privilege = 1)
    if (keep_admin) {
        if (!exec_sql("DELETE FROM users WHERE privilege = 0;", "Clear Users (Keep Admin)")) success = false;
    } else {
        if (!exec_sql("DELETE FROM users;", "Clear All Users")) success = false;
        // 如果全部删除，可以选择重置一下自增主键序列
        exec_sql("DELETE FROM sqlite_sequence WHERE name='users';", "Reset Users Sequence");
    }

    // 重置考勤表自增主键
    exec_sql("DELETE FROM sqlite_sequence WHERE name='attendance';", "Reset Attendance Sequence");

    // 3. 提交或回滚事务
    if (success) {
        exec_sql("COMMIT;", "Commit Transaction");
        std::cout << "[Data] Successfully deleted employee and attendance data." << std::endl;
    } else {
        exec_sql("ROLLBACK;", "Rollback Transaction");
        std::cerr << "[Data] Failed to delete data, transaction rolled back." << std::endl;
        return false;
    }

    // 4. 清理本地存储的图片文件 (考勤抓拍图片 & 人脸注册图片)
    try {
        // 清理打卡图片
        if (fs::exists(IMAGE_DIR)) {
            for (const auto& entry : fs::directory_iterator(IMAGE_DIR)) {
                if (entry.is_regular_file()) {
                    fs::remove(entry.path());
                }
            }
        }

        // 如果没有保留管理员，把人脸注册目录也全清空
        if (!keep_admin && fs::exists(AVATAR_DIR)) {
            for (const auto& entry : fs::directory_iterator(AVATAR_DIR)) {
                if (entry.is_regular_file()) {
                    fs::remove(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Data] Error clearing image files: " << e.what() << std::endl;
    }

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
        "  SUM(CASE WHEN face_feature IS NOT NULL THEN 1 ELSE 0 END), "        // 2: 录入人脸数
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
DbTextLookupResult db_find_system_config(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁
    if (db == nullptr) {
        return {DbTextLookupStatus::ReadError, std::nullopt};
    }

    const char* sql = "SELECT config_value FROM system_config WHERE config_key = ?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get System Config Failed: " << sqlite3_errmsg(db) << std::endl;
        return {DbTextLookupStatus::ReadError, std::nullopt};
    }

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_STATIC);

    const int stepResult = sqlite3_step(stmt.get());
    if (stepResult == SQLITE_ROW) {
        const char* val = (const char*)sqlite3_column_text(stmt.get(), 0);
        return {DbTextLookupStatus::Found, std::string(val ? val : "")};
    }

    return stepResult == SQLITE_DONE
        ? DbTextLookupResult{DbTextLookupStatus::NotFound, std::nullopt}
        : DbTextLookupResult{DbTextLookupStatus::ReadError, std::nullopt};
}

std::string db_get_system_config(const std::string& key, const std::string& default_value) {
    DbTextLookupResult result = db_find_system_config(key);
    return result.status == DbTextLookupStatus::Found && result.value
        ? *result.value
        : default_value;
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
DbTextLookupResult db_find_holiday(const std::string& date_str) {
    std::shared_lock<std::shared_mutex> lock(g_db_mutex); // 读操作使用共享锁
    if (db == nullptr) {
        return {DbTextLookupStatus::ReadError, std::nullopt};
    }

    const char* sql = "SELECT name FROM holidays WHERE date_str = ?;";
    ScopedSqliteStmt stmt;

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Get Holiday Failed: " << sqlite3_errmsg(db) << std::endl;
        return {DbTextLookupStatus::ReadError, std::nullopt};
    }

    sqlite3_bind_text(stmt.get(), 1, date_str.c_str(), -1, SQLITE_STATIC);

    const int stepResult = sqlite3_step(stmt.get());
    if (stepResult == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt.get(), 0);
        return {DbTextLookupStatus::Found, std::string(name ? name : "")};
    }

    return stepResult == SQLITE_DONE
        ? DbTextLookupResult{DbTextLookupStatus::NotFound, std::nullopt}
        : DbTextLookupResult{DbTextLookupStatus::ReadError, std::nullopt};
}

std::optional<std::string> db_get_holiday(const std::string& date_str) {
    DbTextLookupResult result = db_find_holiday(date_str);
    return result.status == DbTextLookupStatus::Found
        ? std::move(result.value)
        : std::nullopt;
}

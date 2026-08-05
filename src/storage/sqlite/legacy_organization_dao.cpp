/**
 * @file
 * @brief 承接旧部门、班次和考勤规则 SQLite DAO。
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

// ================= 1. 部门管理 DAO =================

bool db_add_department(const std::string& dept_name) {
    // 默认使用公司ID=1
    return db_add_department_with_company(dept_name, 1);
}

bool db_add_department_with_company(const std::string& dept_name, int company_id) {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    ScopedSqliteStmt stmt;

    const char* sql = "INSERT INTO departments (name, company_id) VALUES (?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt.get(), 1, dept_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 2, company_id);

    bool ok = (sqlite3_step(stmt.get()) == SQLITE_DONE);

    return ok;
}

std::vector<DeptInfo> db_get_departments() {
    // 获取所有部门（默认公司ID=1）
    return db_get_departments_by_company(1);
}

std::vector<DeptInfo> db_get_departments_by_company(int company_id) {

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<DeptInfo> list;
    ScopedSqliteStmt stmt;

    const char* sql = "SELECT id, name, company_id FROM departments WHERE company_id = ? ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt.get(), 1, company_id);
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            DeptInfo d;
            d.id = sqlite3_column_int(stmt.get(), 0);
            const char* txt = (const char*)sqlite3_column_text(stmt.get(), 1);
            d.name = txt ? txt : "";
            d.company_id = sqlite3_column_int(stmt.get(), 2);
            list.push_back(d);
        }
    }

    return list;
}

std::vector<DeptInfo> db_get_all_departments_with_company() {

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);//共享锁

    std::vector<DeptInfo> list;
    ScopedSqliteStmt stmt;

    const char* sql = "SELECT d.id, d.name, d.company_id, c.name FROM departments d "
                      "LEFT JOIN companies c ON d.company_id = c.id "
                      "ORDER BY c.name, d.name;";

    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) == SQLITE_OK) {
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            DeptInfo d;
            d.id = sqlite3_column_int(stmt.get(), 0);
            const char* txt = (const char*)sqlite3_column_text(stmt.get(), 1);
            d.name = txt ? txt : "";
            d.company_id = sqlite3_column_int(stmt.get(), 2);
            const char* company_name = (const char*)sqlite3_column_text(stmt.get(), 3);
            d.company_name = company_name ? company_name : "";
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

// 更新部门名称
bool db_update_department(int dept_id, const std::string& new_name) {

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);//排他锁（写锁）

    const char* sql = "UPDATE departments SET name=? WHERE id=?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update Dept Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, new_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, dept_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (ok) {
        std::cout << "[Data] Department " << dept_id << " updated to: " << new_name << std::endl;
    }

    return ok;
}


// ================= 2. 班次管理 DAO =================

bool db_update_shift(int shift_id, const std::string& s1_start, const std::string& s1_end,
                     const std::string& s2_start, const std::string& s2_end,
                     const std::string& s3_start, const std::string& s3_end,
                     int cross_day) {
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);

    const char* sql = "INSERT OR REPLACE INTO shifts (id, name, s1_start, s1_end, s2_start, s2_end, s3_start, s3_end, cross_day) "
                      "VALUES (?, "
                      "(SELECT name FROM shifts WHERE id = ?), " // 保持原有的班次名称（如果存在）
                      "?, ?, ?, ?, ?, ?, ?);";

    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
        std::cerr << "[Data] Prepare Update Shift Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 绑定数据
    sqlite3_bind_int(stmt.get(), 1, shift_id);         // 插入的 id
    sqlite3_bind_int(stmt.get(), 2, shift_id);         // 用于子查询 name 的 id
    sqlite3_bind_text(stmt.get(), 3, s1_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 4, s1_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 5, s2_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 6, s2_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 7, s3_start.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 8, s3_end.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(),  9, cross_day);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::cerr << "[Data] Execute Update Shift Failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 如果子查询没查到原名称，导致 name 变成了 NULL，这里做个兜底更新
    const char* fix_name_sql = "UPDATE shifts SET name = '班次' || CAST(id AS TEXT) WHERE id = ? AND name IS NULL;";
    ScopedSqliteStmt stmt_fix;
    if (sqlite3_prepare_v2(db, fix_name_sql, -1, stmt_fix.ptr(), 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt_fix.get(), 1, shift_id);
        sqlite3_step(stmt_fix.get());
    }

    return true;
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

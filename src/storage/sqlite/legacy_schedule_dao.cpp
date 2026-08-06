#include "infrastructure/logging/logger.h"
/**
 * @file
 * @brief 承接旧排班 SQLite DAO。
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
            SA_LOG_ERROR_STREAM() << "[Data] Invalid shift_id: " << entry.shift_id
                      << " for dept_id: " << entry.dept_id
                      << ", day: " << entry.day_of_week << std::endl;
            continue;
        }

        // 使用INSERT OR REPLACE确保一个部门一天只有一条记录
        const char* sql = "INSERT OR REPLACE INTO dept_schedule (dept_id, day_of_week, shift_id) VALUES (?, ?, ?);";
        ScopedSqliteStmt stmt;

        if (sqlite3_prepare_v2(db, sql, -1, stmt.ptr(), 0) != SQLITE_OK) {
            SA_LOG_ERROR_STREAM() << "[Data] Prepare import schedule failed: " << sqlite3_errmsg(db) << std::endl;
            continue;
        }

        sqlite3_bind_int(stmt.get(), 1, entry.dept_id);
        sqlite3_bind_int(stmt.get(), 2, entry.day_of_week);
        sqlite3_bind_int(stmt.get(), 3, entry.shift_id);

        if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
            success_count++;
        } else {
            SA_LOG_ERROR_STREAM() << "[Data] Execute import schedule failed: " << sqlite3_errmsg(db) << std::endl;
        }
    }

    // 提交事务
    if (!exec_sql("COMMIT;", "Commit transaction for import schedules")) {
        // 回滚
        exec_sql("ROLLBACK;", "Rollback transaction for import schedules");
        return 0;
    }

    SA_LOG_INFO_STREAM() << "[Data] Imported " << success_count << " department schedule entries." << std::endl;
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
        SA_LOG_ERROR_STREAM() << "[Data] Prepare get dept schedule failed: " << sqlite3_errmsg(db) << std::endl;
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
        SA_LOG_ERROR_STREAM() << "[Data] Prepare get all shifts limited failed: " << sqlite3_errmsg(db) << std::endl;
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

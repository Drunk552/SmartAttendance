#include "infrastructure/logging/logger.h"
/**
 * @file
 * @brief 承接旧考勤记录 SQLite DAO。
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

// ================= 4. 考勤记录 DAO =================

bool db_log_attendance(int user_id, int shift_id, const cv::Mat& image, int status) {
    return db_log_attendance_at(
        user_id, shift_id, image, status, static_cast<long long>(std::time(nullptr)));
}

bool db_log_attendance_at(int user_id,
                          int shift_id,
                          const cv::Mat& image,
                          int status,
                          long long timestamp) {
    if (timestamp < 0) {
        return false;
    }

    std::string path_str = "";

    // 1. 无需数据库的操作 (保存图片到磁盘) —— 不加锁，不阻塞别人！
    if (!image.empty()) {
        std::string fname = std::to_string(timestamp) + "_" + std::to_string(user_id) + ".jpg";
        fs::path p = fs::path(IMAGE_DIR) / fname;
        try {
            if (cv::imwrite(p.string(), image)) {
                path_str = p.string();
            }
        } catch (...) {
            SA_LOG_ERROR_STREAM() << "[Data] Save Image Failed." << std::endl;
        }
    }

    // 2. 纯粹的数据库插入操作 —— 用大括号包围，精确加锁！
    bool ok = false;
    {
        std::unique_lock<std::shared_mutex> lock(g_db_mutex);// 开始排他锁

        if (!g_stmt_log_attendance) {
            SA_LOG_ERROR_STREAM() << "[Data] Error: log_attendance statement is not precompiled!" << std::endl;
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
        sqlite3_bind_int64(g_stmt_log_attendance, 4, timestamp);
        sqlite3_bind_int(g_stmt_log_attendance, 5, status);

        ok = (sqlite3_step(g_stmt_log_attendance) == SQLITE_DONE);
    } // 离开大括号，排他锁瞬间释放！

    if(ok) {
        SA_LOG_INFO_STREAM() << "[Data] Attendance Logged -> User: " << user_id
                  << " Time: " << timestamp << " Status: " << static_cast<int>(status) << std::endl;
    } else {
        SA_LOG_ERROR_STREAM() << "[Data] Attendance Logged Failed: " << sqlite3_errmsg(db) << std::endl;
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
        SA_LOG_ERROR_STREAM() << "[DB Error] 清理查询失败: " << sqlite3_errmsg(db) << std::endl;
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
            SA_LOG_ERROR_STREAM() << "[Warning] 删除过时图片失败: " << e.what() << std::endl;
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

DbAttendanceQueryResult db_query_records_limited(
    int user_id, long long start_ts, long long end_ts, std::size_t limit,
    std::size_t offset) {
    if (start_ts > end_ts || limit == 0 || limit > kMaxDbAttendanceQuerySize) {
        return {DbAttendanceQueryStatus::InvalidArgument, {}, false};
    }

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    if (!db) return {DbAttendanceQueryStatus::ReadError, {}, false};

    const bool allUsers = user_id < 0;
    const char* sqlAll =
        "SELECT a.id, a.user_id, u.name, d.name, a.timestamp, a.status, a.image_path "
        "FROM attendance a LEFT JOIN users u ON a.user_id = u.id "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE a.timestamp BETWEEN ? AND ? ORDER BY a.timestamp DESC LIMIT ? OFFSET ?;";
    const char* sqlUser =
        "SELECT a.id, a.user_id, u.name, d.name, a.timestamp, a.status, a.image_path "
        "FROM attendance a LEFT JOIN users u ON a.user_id = u.id "
        "LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE a.user_id = ? AND a.timestamp BETWEEN ? AND ? "
        "ORDER BY a.timestamp DESC LIMIT ? OFFSET ?;";

    ScopedSqliteStmt stmt;
    if (sqlite3_prepare_v2(db, allUsers ? sqlAll : sqlUser, -1, stmt.ptr(), 0) != SQLITE_OK) {
        return {DbAttendanceQueryStatus::ReadError, {}, false};
    }
    int index = 1;
    if (!allUsers) sqlite3_bind_int(stmt.get(), index++, user_id);
    sqlite3_bind_int64(stmt.get(), index++, start_ts);
    sqlite3_bind_int64(stmt.get(), index++, end_ts);
    sqlite3_bind_int64(stmt.get(), index++, static_cast<sqlite3_int64>(limit + 1));
    sqlite3_bind_int64(stmt.get(), index, static_cast<sqlite3_int64>(offset));

    std::vector<AttendanceRecord> records;
    records.reserve(limit + 1);
    int step = SQLITE_ROW;
    while ((step = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        AttendanceRecord record{};
        record.id = sqlite3_column_int(stmt.get(), 0);
        record.user_id = sqlite3_column_int(stmt.get(), 1);
        const char* userName = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        const char* departmentName = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        record.user_name = userName ? userName : "Unknown";
        record.dept_name = departmentName ? departmentName : "No Dept";
        record.timestamp = sqlite3_column_int64(stmt.get(), 4);
        record.status = sqlite3_column_int(stmt.get(), 5);
        const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 6));
        record.image_path = imagePath ? imagePath : "";
        record.minutes_late = 0;
        record.minutes_early = 0;
        records.push_back(std::move(record));
    }
    if (step != SQLITE_DONE) return {DbAttendanceQueryStatus::ReadError, {}, false};
    const bool hasMore = records.size() > limit;
    if (hasMore) records.resize(limit);
    return {DbAttendanceQueryStatus::Success, std::move(records), hasMore};
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
        SA_LOG_ERROR_STREAM() << "[Data] Prepare Get Records By User Failed: " << sqlite3_errmsg(db) << std::endl;
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
        SA_LOG_ERROR_STREAM() << "[Data] Prepare Get All Records Failed: " << sqlite3_errmsg(db) << std::endl;
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

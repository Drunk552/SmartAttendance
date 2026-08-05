/**
 * @file legacy_db_internal.h
 * @brief 旧 db_storage 拆分期间共享的 SQLite 连接内部边界。
 *
 * 仅供 storage/sqlite 过渡实现使用；业务层和 UI 不得包含本头文件。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_DB_INTERNAL_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_DB_INTERNAL_H

#include <sqlite3.h>

#include <shared_mutex>
#include <string>

extern sqlite3* db;
extern sqlite3_stmt* g_stmt_log_attendance;
extern std::shared_mutex g_db_mutex;

extern const std::string IMAGE_DIR;
extern const std::string AVATAR_DIR;
extern const std::string DB_NAME;

class ScopedSqliteStmt final {
public:
    explicit ScopedSqliteStmt(sqlite3_stmt* statement = nullptr) noexcept
        : statement_(statement) {}

    ~ScopedSqliteStmt() {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    sqlite3_stmt* get() const noexcept {
        return statement_;
    }

    sqlite3_stmt** ptr() noexcept {
        return &statement_;
    }

    ScopedSqliteStmt(const ScopedSqliteStmt&) = delete;
    ScopedSqliteStmt& operator=(const ScopedSqliteStmt&) = delete;

private:
    sqlite3_stmt* statement_;
};

bool exec_sql(const char* sql, const char* tag);

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_DB_INTERNAL_H

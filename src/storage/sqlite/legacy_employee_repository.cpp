/**
 * @file legacy_employee_repository.cpp
 * @brief 实现 db_storage 员工数据到领域模型的过渡映射。
 */

#include "legacy_employee_repository.h"

#include "data/db_storage.h"
#include "storage/sqlite/legacy_db_internal.h"

#include <sqlite3.h>
#include <utility>
#include <shared_mutex>

namespace smart_attendance::storage::sqlite {

static_assert(kMaxEmployeePageSize == kMaxDbUserBasicPageSize,
              "employee page limits must remain aligned across the adapter boundary");

Result<std::optional<EmployeeDisplayDetails>, RepositoryError>
LegacyEmployeeRepository::findDisplayDetailsById(int employeeId) {
    using ResultType =
        Result<std::optional<EmployeeDisplayDetails>, RepositoryError>;
    if (employeeId <= 0 || !data_is_open()) {
        return ResultType::failure(
            employeeId <= 0 ? RepositoryError::InvalidArgument
                            : RepositoryError::ReadFailed);
    }

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql =
        "SELECT u.id, u.name, u.dept_id, u.privilege, "
        "d.name, COALESCE(u.avatar_path, ''), "
        "CASE WHEN length(u.fingerprint_data) > 0 THEN 1 ELSE 0 END, "
        "COALESCE(u.card_id, ''), "
        "CASE WHEN length(u.password) > 0 THEN 1 ELSE 0 END "
        "FROM users u LEFT JOIN departments d ON u.dept_id = d.id "
        "WHERE u.id = ?;";

    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 1, employeeId) != SQLITE_OK) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    const int stepResult = sqlite3_step(statement.get());
    if (stepResult == SQLITE_DONE) {
        return ResultType::success(std::nullopt);
    }
    if (stepResult != SQLITE_ROW) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    const int roleValue = sqlite3_column_int(statement.get(), 3);
    core::EmployeeRole role;
    if (roleValue == 0) {
        role = core::EmployeeRole::Regular;
    } else if (roleValue == 1) {
        role = core::EmployeeRole::Administrator;
    } else {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    const char* name = reinterpret_cast<const char*>(
        sqlite3_column_text(statement.get(), 1));
    const char* departmentName = reinterpret_cast<const char*>(
        sqlite3_column_text(statement.get(), 4));
    const char* avatarPath = reinterpret_cast<const char*>(
        sqlite3_column_text(statement.get(), 5));
    const char* cardId = reinterpret_cast<const char*>(
        sqlite3_column_text(statement.get(), 7));

    return ResultType::success(EmployeeDisplayDetails{
        core::Employee{
            sqlite3_column_int(statement.get(), 0),
            name ? name : "",
            sqlite3_column_int(statement.get(), 2),
            role},
        departmentName ? departmentName : "Unknown",
        avatarPath && *avatarPath != '\0',
        sqlite3_column_int(statement.get(), 6) != 0,
        cardId ? cardId : "",
        sqlite3_column_int(statement.get(), 8) != 0});
}

Result<void, RepositoryError> LegacyEmployeeRepository::updateName(
    int employeeId, const std::string& name) {
    using ResultType = Result<void, RepositoryError>;
    if (employeeId <= 0 || name.empty()) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE users SET name = ? WHERE id = ?;";
    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_text(statement.get(), 1, name.c_str(), -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 2, employeeId) != SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, RepositoryError> LegacyEmployeeRepository::updateDepartment(
    int employeeId, int departmentId) {
    using ResultType = Result<void, RepositoryError>;
    if (employeeId <= 0 || departmentId <= 0) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE users SET dept_id = ? WHERE id = ?;";
    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 1, departmentId) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 2, employeeId) != SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, RepositoryError> LegacyEmployeeRepository::updateRole(
    int employeeId, int role) {
    using ResultType = Result<void, RepositoryError>;
    if (employeeId <= 0 || (role != 0 && role != 1)) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE users SET privilege = ? WHERE id = ?;";
    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 1, role) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 2, employeeId) != SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<PasswordVerification, RepositoryError>
LegacyEmployeeRepository::verifyPassword(
    int employeeId, const std::string& password) {
    using ResultType = Result<PasswordVerification, RepositoryError>;
    if (employeeId <= 0 || password.empty()) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    std::shared_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql = "SELECT password FROM users WHERE id = ?;";
    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 1, employeeId) != SQLITE_OK) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    const int stepResult = sqlite3_step(statement.get());
    if (stepResult == SQLITE_DONE) {
        return ResultType::success(PasswordVerification::NotFound);
    }
    if (stepResult != SQLITE_ROW) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    const char* storedText = reinterpret_cast<const char*>(
        sqlite3_column_text(statement.get(), 0));
    const std::string stored = storedText ? storedText : "";
    if (stored.empty()) {
        return ResultType::success(PasswordVerification::NotConfigured);
    }
    if (stored == password || stored == db_hash_password(password)) {
        return ResultType::success(PasswordVerification::Match);
    }
    return ResultType::success(PasswordVerification::Mismatch);
}

Result<bool, RepositoryError> LegacyEmployeeRepository::updatePassword(
    int employeeId, const std::string& password) {
    using ResultType = Result<bool, RepositoryError>;
    if (employeeId <= 0 || password.empty()) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    const std::string hashedPassword = db_hash_password(password);
    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE users SET password = ? WHERE id = ?;";
    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_text(statement.get(), 1, hashedPassword.c_str(), -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 2, employeeId) != SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success(sqlite3_changes(db) > 0);
}

Result<bool, RepositoryError> LegacyEmployeeRepository::remove(int employeeId) {
    using ResultType = Result<bool, RepositoryError>;
    if (employeeId <= 0) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    const char* sql = "DELETE FROM users WHERE id = ?;";
    ScopedSqliteStmt statement;
    if (sqlite3_prepare_v2(db, sql, -1, statement.ptr(), nullptr) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 1, employeeId) != SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success(sqlite3_changes(db) > 0);
}

Result<std::optional<core::Employee>, RepositoryError>
LegacyEmployeeRepository::findById(int employeeId) {
    using ResultType = Result<std::optional<core::Employee>, RepositoryError>;
    try {
        DbUserLookupResult lookup = db_find_user_info(employeeId);
        if (lookup.status == DbUserLookupStatus::ReadError) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }
        if (lookup.status == DbUserLookupStatus::NotFound) {
            return ResultType::success(std::nullopt);
        }

        const auto& user = lookup.user;
        if (!user) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }

        core::EmployeeRole role;
        if (user->role == 0) {
            role = core::EmployeeRole::Regular;
        } else if (user->role == 1) {
            role = core::EmployeeRole::Administrator;
        } else {
            return ResultType::failure(RepositoryError::ReadFailed);
        }

        return ResultType::success(core::Employee{
            user->id,
            user->name,
            user->dept_id,
            role});
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

Result<EmployeePage, RepositoryError>
LegacyEmployeeRepository::listPage(std::size_t offset, std::size_t limit) {
    using ResultType = Result<EmployeePage, RepositoryError>;
    if (limit == 0 || limit > kMaxEmployeePageSize) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }

    try {
        DbUserPageResult page = db_find_user_basics_page(offset, limit);
        if (page.status == DbUserPageStatus::InvalidArgument) {
            return ResultType::failure(RepositoryError::InvalidArgument);
        }
        if (page.status == DbUserPageStatus::ReadError) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }

        std::vector<EmployeePage::Entry> employees;
        employees.reserve(page.users.size());
        for (const UserData& user : page.users) {
            core::EmployeeRole role;
            if (user.role == 0) {
                role = core::EmployeeRole::Regular;
            } else if (user.role == 1) {
                role = core::EmployeeRole::Administrator;
            } else {
                return ResultType::failure(RepositoryError::ReadFailed);
            }
            employees.push_back(EmployeePage::Entry{
                core::Employee{
                    user.id,
                    user.name,
                    user.dept_id,
                    role},
                user.dept_name});
        }

        return ResultType::success(EmployeePage{
            std::move(employees),
            page.has_more});
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

} // namespace smart_attendance::storage::sqlite

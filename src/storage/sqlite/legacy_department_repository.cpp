/**
 * @file legacy_department_repository.cpp
 * @brief 实现旧部门维护和排班读取 DAO 到领域接口的映射。
 */

#include "legacy_department_repository.h"

#include "data/db_storage.h"
#include "storage/sqlite/legacy_db_internal.h"

#include <mutex>
#include <utility>

namespace smart_attendance::storage::sqlite {

Result<void, RepositoryError>
LegacyDepartmentRepository::addDefaultCompanyDepartment(const std::string& name) {
    using ResultType = Result<void, RepositoryError>;
    if (name.empty()) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open() || !db_add_department(name)) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, RepositoryError>
LegacyDepartmentRepository::renameDepartment(
    int departmentId, const std::string& name) {
    using ResultType = Result<void, RepositoryError>;
    if (departmentId <= 0 || name.empty()) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open() || !db_update_department(departmentId, name)) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, RepositoryError>
LegacyDepartmentRepository::removeDepartment(int departmentId) {
    using ResultType = Result<void, RepositoryError>;
    if (departmentId <= 0) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open() || !db_delete_department(departmentId)) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, RepositoryError> LegacyDepartmentRepository::updateSchedule(
    int departmentId,
    const std::string& departmentName,
    const std::array<int, 7>& shiftIds) {
    using ResultType = Result<void, RepositoryError>;
    if (departmentId <= 0 || departmentName.empty()) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    for (int shiftId : shiftIds) {
        if (shiftId < ScheduleConstants::HOLIDAY ||
            shiftId > ScheduleConstants::MAX_SHIFT_ID) {
            return ResultType::failure(RepositoryError::InvalidArgument);
        }
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    std::unique_lock<std::shared_mutex> lock(g_db_mutex);
    if (!exec_sql("BEGIN TRANSACTION;", "Begin department schedule update")) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    const auto rollback = []() {
        exec_sql("ROLLBACK;", "Rollback department schedule update");
    };

    ScopedSqliteStmt updateDepartment;
    const char* updateSql = "UPDATE departments SET name = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, updateSql, -1, updateDepartment.ptr(), nullptr) !=
        SQLITE_OK) {
        rollback();
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    sqlite3_bind_text(
        updateDepartment.get(), 1, departmentName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(updateDepartment.get(), 2, departmentId);
    if (sqlite3_step(updateDepartment.get()) != SQLITE_DONE) {
        rollback();
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    ScopedSqliteStmt clearSchedule;
    const char* clearSql = "DELETE FROM dept_schedule WHERE dept_id = ?;";
    if (sqlite3_prepare_v2(db, clearSql, -1, clearSchedule.ptr(), nullptr) !=
        SQLITE_OK) {
        rollback();
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    sqlite3_bind_int(clearSchedule.get(), 1, departmentId);
    if (sqlite3_step(clearSchedule.get()) != SQLITE_DONE) {
        rollback();
        return ResultType::failure(RepositoryError::WriteFailed);
    }

    const char* insertSql =
        "INSERT INTO dept_schedule (dept_id, day_of_week, shift_id) "
        "VALUES (?, ?, ?);";
    for (std::size_t day = 0; day < shiftIds.size(); ++day) {
        if (shiftIds[day] == ScheduleConstants::HOLIDAY) {
            continue;
        }
        ScopedSqliteStmt insertSchedule;
        if (sqlite3_prepare_v2(
                db, insertSql, -1, insertSchedule.ptr(), nullptr) != SQLITE_OK) {
            rollback();
            return ResultType::failure(RepositoryError::WriteFailed);
        }
        sqlite3_bind_int(insertSchedule.get(), 1, departmentId);
        sqlite3_bind_int(insertSchedule.get(), 2, static_cast<int>(day));
        sqlite3_bind_int(insertSchedule.get(), 3, shiftIds[day]);
        if (sqlite3_step(insertSchedule.get()) != SQLITE_DONE) {
            rollback();
            return ResultType::failure(RepositoryError::WriteFailed);
        }
    }

    if (!exec_sql("COMMIT;", "Commit department schedule update")) {
        rollback();
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<std::vector<core::Department>, RepositoryError>
LegacyDepartmentRepository::listDefaultCompany() {
    using ResultType = Result<std::vector<core::Department>, RepositoryError>;
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    try {
        const auto departments = db_get_departments();
        std::vector<core::Department> result;
        result.reserve(departments.size());
        for (const auto& department : departments) {
            result.push_back(core::Department{
                department.id, department.name, department.company_id});
        }
        return ResultType::success(std::move(result));
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

Result<int, RepositoryError>
LegacyDepartmentRepository::countEmployees(int departmentId) {
    using ResultType = Result<int, RepositoryError>;
    if (departmentId <= 0) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    try {
        const auto count = db_count_users_by_department(departmentId);
        if (!count) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }
        return ResultType::success(*count);
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

Result<std::optional<core::DepartmentSchedule>, RepositoryError>
LegacyDepartmentRepository::findSchedule(int departmentId) {
    using ResultType =
        Result<std::optional<core::DepartmentSchedule>, RepositoryError>;
    if (departmentId <= 0) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    try {
        const auto view = db_get_dept_schedule_view(departmentId);
        if (view.dept_name.empty()) {
            return ResultType::success(std::nullopt);
        }
        core::DepartmentSchedule schedule{
            view.dept_id,
            view.dept_name,
            {view.shifts[0], view.shifts[1], view.shifts[2], view.shifts[3],
             view.shifts[4], view.shifts[5], view.shifts[6]}};
        return ResultType::success(std::move(schedule));
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

} // namespace smart_attendance::storage::sqlite

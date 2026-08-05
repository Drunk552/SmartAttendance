#include "core/model/employee.h"
#include "data/db_storage.h"
#include "storage/sqlite/legacy_employee_repository.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <unistd.h>

namespace {

using smart_attendance::core::EmployeeRole;
using smart_attendance::storage::kMaxEmployeePageSize;
using smart_attendance::storage::RepositoryError;
using smart_attendance::storage::sqlite::LegacyEmployeeRepository;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void removeUsersTable() {
    sqlite3* connection = nullptr;
    require(sqlite3_open("attendance.db", &connection) == SQLITE_OK,
            "fault-injection database connection should open");
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(connection, "DROP TABLE users;", nullptr, nullptr,
                                    &errorMessage);
    if (errorMessage != nullptr) {
        sqlite3_free(errorMessage);
    }
    sqlite3_close(connection);
    require(result == SQLITE_OK, "users table should be removed for SQL fault injection");
}

class TemporaryDatabaseDirectory final {
public:
    TemporaryDatabaseDirectory()
        : originalDirectory_(std::filesystem::current_path()) {
        char pathTemplate[] = "/tmp/smart_attendance_employee_XXXXXX";
        char* created = ::mkdtemp(pathTemplate);
        require(created != nullptr, "temporary database directory should be created");
        path_ = created;
        std::filesystem::current_path(path_);
    }

    ~TemporaryDatabaseDirectory() {
        data_close();
        std::error_code error;
        std::filesystem::current_path(originalDirectory_, error);
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDatabaseDirectory(const TemporaryDatabaseDirectory&) = delete;
    TemporaryDatabaseDirectory& operator=(const TemporaryDatabaseDirectory&) = delete;

private:
    std::filesystem::path originalDirectory_;
    std::filesystem::path path_;
};

void testFindById() {
    TemporaryDatabaseDirectory environment;
    require(data_init(), "temporary database should initialize");

    LegacyEmployeeRepository repository;
    const auto found = repository.findById(1);
    require(found && found.value().has_value(),
            "seed administrator should be returned by employee repository");
    const auto& employee = *found.value();
    require(employee.id == 1 && employee.name == "SuperAdmin",
            "employee identity should match seeded database values");
    require(employee.departmentId > 0,
            "employee department should be mapped without exposing legacy UserData");
    require(employee.role == EmployeeRole::Administrator,
            "employee privilege should map to the strong role enum");

    const auto display = repository.findDisplayDetailsById(1);
    require(display && display.value().has_value(),
            "employee display details should be returned for the seeded administrator");
    require(display.value()->employee.name == "SuperAdmin" &&
                display.value()->employee.role == EmployeeRole::Administrator &&
                display.value()->faceRegistered &&
                display.value()->fingerprintRegistered == false &&
                display.value()->cardId == "000000" &&
                display.value()->passwordRegistered,
            "display details should expose status values without credential blobs");
    const auto legacyPassword = repository.verifyPassword(1, "888888");
    require(legacyPassword &&
                legacyPassword.value() ==
                    smart_attendance::storage::PasswordVerification::Match,
            "seeded legacy plaintext password should remain compatible");
    require(repository.updatePassword(1, "123456").value(),
            "password update should affect the seeded administrator");
    const auto hashedPassword = repository.verifyPassword(1, "123456");
    require(hashedPassword &&
                hashedPassword.value() ==
                    smart_attendance::storage::PasswordVerification::Match,
            "updated hashed password should be verifiable");

    UserData regularUser;
    regularUser.name = "RegularEmployee";
    regularUser.dept_id = employee.departmentId;
    regularUser.role = 0;
    regularUser.password = "password-marker";
    regularUser.card_id = "CARD-MARKER";
    require(db_add_user(regularUser, cv::Mat{}) > 0,
            "second employee should be inserted for pagination verification");

    const auto firstPage = repository.listPage(0, 1);
    require(firstPage && firstPage.value().employees.size() == 1,
            "first employee page should respect the requested bound");
    require(firstPage.value().employees.front().employee.id == 1 &&
                firstPage.value().hasMore,
            "employee pages should be ordered by id and report a following page");
    require(!firstPage.value().employees.front().departmentName.empty(),
            "employee list entries should include the joined department name");

    const auto secondPage = repository.listPage(1, 1);
    require(secondPage && secondPage.value().employees.size() == 1 &&
                !secondPage.value().hasMore,
            "last employee page should report that no further rows exist");
    require(secondPage.value().employees.front().employee.name ==
                "RegularEmployee" &&
                secondPage.value().employees.front().employee.role ==
                    EmployeeRole::Regular,
            "employee page should map only basic domain fields");

    require(repository.updateName(2, "RenamedEmployee").hasValue(),
            "employee repository should update only the employee name");
    const int updatedDepartmentId = employee.departmentId + 1;
    require(repository.updateDepartment(2, updatedDepartmentId).hasValue(),
            "employee repository should update only the employee department");
    const auto updatedDisplay = repository.findDisplayDetailsById(2);
    require(updatedDisplay && updatedDisplay.value().has_value() &&
                updatedDisplay.value()->employee.name == "RenamedEmployee" &&
                updatedDisplay.value()->employee.departmentId == updatedDepartmentId &&
                updatedDisplay.value()->cardId == "CARD-MARKER" &&
                updatedDisplay.value()->passwordRegistered,
            "basic updates should preserve unrelated employee credentials");
    require(repository.updateRole(2, 1).hasValue(),
            "employee repository should update only the privilege field");
    const auto updatedRole = repository.findDisplayDetailsById(2);
    require(updatedRole && updatedRole.value().has_value() &&
                updatedRole.value()->employee.role == EmployeeRole::Administrator &&
                updatedRole.value()->cardId == "CARD-MARKER" &&
                updatedRole.value()->passwordRegistered,
            "role update should preserve unrelated employee credentials");

    sqlite3* cascadeConnection = nullptr;
    require(sqlite3_open("attendance.db", &cascadeConnection) == SQLITE_OK,
            "cascade verification database connection should open");
    char* cascadeError = nullptr;
    require(sqlite3_exec(
                cascadeConnection,
                "INSERT INTO attendance(user_id, timestamp, status) "
                "VALUES(2, 1, 0);"
                "INSERT INTO user_schedule(user_id, date_str, shift_id) "
                "VALUES(2, '2026-08-05', 1);",
                nullptr, nullptr, &cascadeError) == SQLITE_OK,
            "dependent employee rows should be inserted for cascade verification");
    if (cascadeError != nullptr) {
        sqlite3_free(cascadeError);
    }
    sqlite3_close(cascadeConnection);

    require(repository.remove(2).value(),
            "existing employee should be removed by the repository");
    const auto removed = repository.findById(2);
    require(removed && !removed.value(),
            "removed employee should no longer be queryable");
    const auto removedAgain = repository.remove(2);
    require(removedAgain && !removedAgain.value(),
            "removing a missing employee should report false without SQL failure");

    const auto invalidZeroLimit = repository.listPage(0, 0);
    const auto invalidLargeLimit = repository.listPage(0, kMaxEmployeePageSize + 1);
    require(!invalidZeroLimit &&
                invalidZeroLimit.error() == RepositoryError::InvalidArgument &&
                !invalidLargeLimit &&
                invalidLargeLimit.error() == RepositoryError::InvalidArgument,
            "employee repository must reject unbounded page requests");
    const auto invalidNameUpdate = repository.updateName(0, "Name");
    require(!invalidNameUpdate &&
                invalidNameUpdate.error() == RepositoryError::InvalidArgument,
            "employee repository should reject invalid name update ids");
    const auto invalidDepartmentUpdate = repository.updateDepartment(2, 0);
    require(!invalidDepartmentUpdate &&
                invalidDepartmentUpdate.error() == RepositoryError::InvalidArgument,
            "employee repository should reject invalid department ids");
    const auto invalidRoleUpdate = repository.updateRole(2, 2);
    require(!invalidRoleUpdate &&
                invalidRoleUpdate.error() == RepositoryError::InvalidArgument,
            "employee repository should reject invalid role values");

    const auto missing = repository.findById(999999);
    require(missing && !missing.value(),
            "missing employee should be a successful empty result");
    const auto missingDisplay = repository.findDisplayDetailsById(999999);
    require(missingDisplay && !missingDisplay.value(),
            "missing display details should remain a successful empty result");

    removeUsersTable();
    const auto sqlFailure = repository.findById(1);
    require(!sqlFailure && sqlFailure.error() == RepositoryError::ReadFailed,
            "SQL read failure must not be reported as a missing employee");
    const auto pageSqlFailure = repository.listPage(0, 1);
    require(!pageSqlFailure &&
                pageSqlFailure.error() == RepositoryError::ReadFailed,
            "employee page SQL failure must be explicit");
    const auto displaySqlFailure = repository.findDisplayDetailsById(1);
    require(!displaySqlFailure &&
                displaySqlFailure.error() == RepositoryError::ReadFailed,
            "display detail SQL failure must be explicit");

    data_close();
    const auto unavailable = repository.findById(1);
    require(!unavailable && unavailable.error() == RepositoryError::ReadFailed,
            "closed database should be an explicit repository read failure");
    const auto unavailablePage = repository.listPage(0, 1);
    require(!unavailablePage &&
                unavailablePage.error() == RepositoryError::ReadFailed,
            "closed database should fail employee page reads explicitly");
    const auto unavailableDisplay = repository.findDisplayDetailsById(1);
    require(!unavailableDisplay &&
                unavailableDisplay.error() == RepositoryError::ReadFailed,
            "closed database should fail employee display reads explicitly");
}

} // namespace

int main() {
    testFindById();
    std::cout << "legacy_employee_repository_test: PASS\n";
    return 0;
}

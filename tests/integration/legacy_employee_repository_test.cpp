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

    UserData regularUser;
    regularUser.name = "RegularEmployee";
    regularUser.dept_id = employee.departmentId;
    regularUser.role = 0;
    require(db_add_user(regularUser, cv::Mat{}) > 0,
            "second employee should be inserted for pagination verification");

    const auto firstPage = repository.listPage(0, 1);
    require(firstPage && firstPage.value().employees.size() == 1,
            "first employee page should respect the requested bound");
    require(firstPage.value().employees.front().id == 1 &&
                firstPage.value().hasMore,
            "employee pages should be ordered by id and report a following page");

    const auto secondPage = repository.listPage(1, 1);
    require(secondPage && secondPage.value().employees.size() == 1 &&
                !secondPage.value().hasMore,
            "last employee page should report that no further rows exist");
    require(secondPage.value().employees.front().name == "RegularEmployee" &&
                secondPage.value().employees.front().role == EmployeeRole::Regular,
            "employee page should map only basic domain fields");

    const auto invalidZeroLimit = repository.listPage(0, 0);
    const auto invalidLargeLimit = repository.listPage(0, kMaxEmployeePageSize + 1);
    require(!invalidZeroLimit &&
                invalidZeroLimit.error() == RepositoryError::InvalidArgument &&
                !invalidLargeLimit &&
                invalidLargeLimit.error() == RepositoryError::InvalidArgument,
            "employee repository must reject unbounded page requests");

    const auto missing = repository.findById(999999);
    require(missing && !missing.value(),
            "missing employee should be a successful empty result");

    removeUsersTable();
    const auto sqlFailure = repository.findById(1);
    require(!sqlFailure && sqlFailure.error() == RepositoryError::ReadFailed,
            "SQL read failure must not be reported as a missing employee");
    const auto pageSqlFailure = repository.listPage(0, 1);
    require(!pageSqlFailure &&
                pageSqlFailure.error() == RepositoryError::ReadFailed,
            "employee page SQL failure must be explicit");

    data_close();
    const auto unavailable = repository.findById(1);
    require(!unavailable && unavailable.error() == RepositoryError::ReadFailed,
            "closed database should be an explicit repository read failure");
    const auto unavailablePage = repository.listPage(0, 1);
    require(!unavailablePage &&
                unavailablePage.error() == RepositoryError::ReadFailed,
            "closed database should fail employee page reads explicitly");
}

} // namespace

int main() {
    testFindById();
    std::cout << "legacy_employee_repository_test: PASS\n";
    return 0;
}

#include "data/db_storage.h"
#include "storage/sqlite/legacy_config_repository.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <unistd.h>

namespace {

using smart_attendance::storage::RepositoryError;
using smart_attendance::storage::sqlite::LegacyConfigRepository;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TemporaryDatabaseDirectory final {
public:
    TemporaryDatabaseDirectory()
        : originalDirectory_(std::filesystem::current_path()) {
        char pathTemplate[] = "/tmp/smart_attendance_config_XXXXXX";
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

private:
    std::filesystem::path originalDirectory_;
    std::filesystem::path path_;
};

void dropTable(const char* table) {
    sqlite3* connection = nullptr;
    require(sqlite3_open("attendance.db", &connection) == SQLITE_OK,
            "fault injection connection should open");
    const std::string sql = std::string("DROP TABLE ") + table + ";";
    require(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
            "fault injection table should be dropped");
    sqlite3_close(connection);
}

void testConfigAndHolidayCrud() {
    TemporaryDatabaseDirectory environment;
    require(data_init(), "temporary database should initialize");
    LegacyConfigRepository repository;

    const auto missing = repository.findValue("phase4.missing");
    require(missing && !missing.value(), "missing config should be successful and empty");
    require(static_cast<bool>(repository.saveValue("phase4.key", "value")),
            "config should save");
    const auto value = repository.findValue("phase4.key");
    require(value && value.value() && *value.value() == "value",
            "saved config should round trip");

    const auto noHoliday = repository.findHoliday("2099-01-02");
    require(noHoliday && !noHoliday.value(), "missing holiday should be successful and empty");
    require(static_cast<bool>(repository.saveHoliday("2099-01-02", "Test Holiday")),
            "holiday should save");
    const auto holiday = repository.findHoliday("2099-01-02");
    require(holiday && holiday.value() && *holiday.value() == "Test Holiday",
            "saved holiday should round trip");
    require(static_cast<bool>(repository.deleteHoliday("2099-01-02")),
            "holiday should delete");

    require(!repository.findValue("") &&
                repository.findValue("").error() == RepositoryError::InvalidArgument,
            "empty config key should be rejected");

    dropTable("system_config");
    const auto readFailure = repository.findValue("phase4.key");
    require(!readFailure && readFailure.error() == RepositoryError::ReadFailed,
            "SQL config failure should be explicit");

    data_close();
    const auto closed = repository.findHoliday("2099-01-02");
    require(!closed && closed.error() == RepositoryError::ReadFailed,
            "closed database should fail reads explicitly");
}

} // namespace

int main() {
    testConfigAndHolidayCrud();
    std::cout << "legacy_config_repository_test: PASS\n";
    return EXIT_SUCCESS;
}

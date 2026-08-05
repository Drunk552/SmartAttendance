#include "data/db_storage.h"
#include "storage/sqlite/legacy_maintenance_repository.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <opencv2/core.hpp>
#include <unistd.h>

namespace {
void require(bool value, const char* message) { if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
class Environment final {
public:
    Environment() : original_(std::filesystem::current_path()) { char value[] = "/tmp/smart_attendance_maintenance_XXXXXX"; char* path = ::mkdtemp(value); require(path != nullptr, "temporary directory should be created"); path_ = path; std::filesystem::current_path(path_); }
    ~Environment() { data_close(); std::error_code error; std::filesystem::current_path(original_, error); std::filesystem::remove_all(path_, error); }
private: std::filesystem::path original_; std::filesystem::path path_;
};
}
int main() {
    Environment environment;
    require(data_init(), "temporary database should initialize");
    smart_attendance::storage::sqlite::LegacyMaintenanceRepository repository;
    require(db_log_attendance_at(1, 1, cv::Mat{}, 0, 100), "record should be inserted");
    require(static_cast<bool>(repository.clearAttendance()), "attendance clear should succeed");
    require(db_get_records(0, 200).empty(), "attendance clear should remove records");
    require(static_cast<bool>(repository.clearEmployees()), "employee clear should succeed");
    require(db_get_all_users().empty(), "employee clear should remove users");
    require(static_cast<bool>(repository.factoryReset()), "factory reset should recreate the database");
    require(!db_get_all_users().empty(), "factory reset should restore seeded administrator");
    require(static_cast<bool>(repository.clearAllData()), "combined clear should preserve legacy sequential behavior");
    data_close();
    require(!repository.clearAttendance(), "closed database should fail maintenance writes");
    std::cout << "legacy_maintenance_repository_test: PASS\n";
}

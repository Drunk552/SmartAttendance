#include "data/db_storage.h"
#include "storage/sqlite/legacy_shift_repository.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace {
void require(bool condition, const char* message) { if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
class TemporaryDatabaseDirectory final {
public:
    TemporaryDatabaseDirectory() : original_(std::filesystem::current_path()) { char value[] = "/tmp/smart_attendance_shift_XXXXXX"; char* path = ::mkdtemp(value); require(path != nullptr, "temporary directory should be created"); path_ = path; std::filesystem::current_path(path_); }
    ~TemporaryDatabaseDirectory() { data_close(); std::error_code error; std::filesystem::current_path(original_, error); std::filesystem::remove_all(path_, error); }
private: std::filesystem::path original_; std::filesystem::path path_;
};
}
int main() {
    TemporaryDatabaseDirectory environment;
    require(data_init(), "temporary database should initialize");
    smart_attendance::storage::sqlite::LegacyShiftRepository repository;
    const auto shifts = repository.listAll();
    require(shifts && !shifts.value().empty() && shifts.value().size() <= 10, "seeded shifts should be returned with a bound");
    auto shift = shifts.value().front();
    shift.thirdStart = "19:00"; shift.thirdEnd = "20:00";
    require(static_cast<bool>(repository.update(shift)), "shift update should succeed");
    const auto saved = repository.findById(shift.id);
    require(saved && saved.value() && saved.value()->thirdStart == "19:00" && saved.value()->thirdEnd == "20:00" && saved.value()->crossDay == 0, "all three periods should round trip");
    const auto missing = repository.findById(9999);
    require(missing && !missing.value(), "missing shift should not be a read failure");
    data_close();
    require(!repository.listAll(), "closed database should fail reads explicitly");
    std::cout << "legacy_shift_repository_test: PASS\n";
}

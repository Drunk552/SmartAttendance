#include "data/db_storage.h"
#include "storage/sqlite/legacy_attendance_repository.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <opencv2/core.hpp>
#include <unistd.h>

namespace {
void require(bool value, const char* message) { if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
class Environment final {
public:
    Environment() : original_(std::filesystem::current_path()) { char value[] = "/tmp/smart_attendance_query_XXXXXX"; char* path = ::mkdtemp(value); require(path != nullptr, "temporary directory should be created"); path_ = path; std::filesystem::current_path(path_); }
    ~Environment() { data_close(); std::error_code error; std::filesystem::current_path(original_, error); std::filesystem::remove_all(path_, error); }
private: std::filesystem::path original_; std::filesystem::path path_;
};
}
int main() {
    Environment environment;
    require(data_init(), "temporary database should initialize");
    require(db_log_attendance_at(1, 1, cv::Mat{}, 0, 100), "first record should be inserted");
    require(db_log_attendance_at(1, 1, cv::Mat{}, 1, 200), "second record should be inserted");
    smart_attendance::storage::sqlite::LegacyAttendanceQueryRepository repository;
    const auto records = repository.query(1, 0, 300, 10, 0);
    require(records && records.value().records.size() == 2 && records.value().records[0].timestamp == 200 && !records.value().hasMore, "query should filter by user and return newest first");
    const auto all = repository.query(-1, 0, 300, 1, 0);
    require(all && all.value().records.size() == 1 && all.value().hasMore, "all-user query should honor the bound");
    const auto secondPage = repository.query(1, 0, 300, 1, 1);
    require(secondPage && secondPage.value().records.size() == 1 && secondPage.value().records[0].timestamp == 100, "offset should return the next page");
    require(!repository.query(1, 300, 0, 10, 0), "invalid range should fail explicitly");
    data_close();
    require(!repository.query(-1, 0, 300, 10, 0), "closed database should fail explicitly");
    std::cout << "legacy_attendance_query_repository_test: PASS\n";
}

#include "data/db_storage.h"
#include "services/punch_service.h"
#include "storage/sqlite/legacy_punch_repositories.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

using smart_attendance::services::PunchError;
using smart_attendance::services::PunchService;
using smart_attendance::storage::sqlite::LegacyAttendanceRepository;
using smart_attendance::storage::sqlite::LegacyAttendanceRuleRepository;
using smart_attendance::storage::sqlite::LegacyScheduleRepository;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class TemporaryDatabaseDirectory final {
public:
    TemporaryDatabaseDirectory()
        : originalDirectory_(std::filesystem::current_path()) {
        char pathTemplate[] = "/tmp/smart_attendance_punch_XXXXXX";
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

std::string localDate(std::time_t timestamp) {
    std::tm local{};
    require(::localtime_r(&timestamp, &local) != nullptr,
            "test timestamp should convert to local date");
    char buffer[11]{};
    require(std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local) == 10,
            "local date should fit YYYY-MM-DD");
    return buffer;
}

void testTemporaryDatabasePunchChain() {
    TemporaryDatabaseDirectory environment;
    require(data_init(), "temporary database should initialize");

    const auto shifts = db_get_shifts();
    require(!shifts.empty(), "database seed should create a shift");

    const std::time_t now = std::time(nullptr);
    const int userId = 1;
    require(db_set_user_special_schedule(userId, localDate(now), shifts.front().id),
            "test user should receive an explicit schedule");

    RuleConfig rules = db_get_global_rules();
    rules.duplicate_punch_limit = 5;
    require(db_update_global_rules(rules), "duplicate rule should update");

    LegacyScheduleRepository schedules;
    LegacyAttendanceRuleRepository ruleRepository;
    LegacyAttendanceRepository attendance;
    PunchService service(schedules, ruleRepository, attendance);

    std::vector<uchar> snapshotJpeg;
    require(cv::imencode(".jpg",
                         cv::Mat::zeros(32, 32, CV_8UC3),
                         snapshotJpeg),
            "test snapshot should encode as JPEG");

    const auto first = service.punch(
        {userId, static_cast<std::int64_t>(now), 8 * 60, snapshotJpeg});
    require(first.hasValue(), "first punch should persist through legacy adapter");

    const auto records = db_get_records_by_user(userId, now, now);
    require(records.size() == 1, "temporary database should contain one punch");
    require(records.front().timestamp == now,
            "adapter must preserve PunchService timestamp");
    require(!records.front().image_path.empty() &&
                std::filesystem::exists(records.front().image_path),
            "adapter must preserve the encoded face snapshot");
    require(first.value().shiftId == shifts.front().id,
            "service receipt should preserve selected shift");

    const auto second = service.punch(
        {userId, static_cast<std::int64_t>(now), 8 * 60});
    require(!second && second.error() == PunchError::DuplicatePunch,
            "second punch in the configured window should be rejected");

    data_close();
    const auto unavailable = schedules.findForUserAt(userId, now);
    require(!unavailable,
            "closed legacy database should map to an explicit read failure");
    require(!ruleRepository.load(),
            "closed legacy database should reject rule reads");
    require(!attendance.save({userId,
                              shifts.front().id,
                              static_cast<std::int64_t>(now),
                              smart_attendance::core::PunchStatus::Normal,
                              0}),
            "closed legacy database should reject writes");
}

} // namespace

int main() {
    testTemporaryDatabasePunchChain();
    std::cout << "legacy_punch_repositories_test: PASS\n";
    return 0;
}

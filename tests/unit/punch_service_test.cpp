#include "services/punch_service.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

using smart_attendance::Result;
using smart_attendance::core::PunchStatus;
using smart_attendance::core::ShiftPeriod;
using smart_attendance::core::calculatePunch;
using smart_attendance::services::PunchError;
using smart_attendance::services::PunchRequest;
using smart_attendance::services::PunchService;
using smart_attendance::storage::AttendanceEntry;
using smart_attendance::storage::AttendanceRules;
using smart_attendance::storage::IAttendanceRepository;
using smart_attendance::storage::IAttendanceRuleRepository;
using smart_attendance::storage::IScheduleRepository;
using smart_attendance::storage::RepositoryError;
using smart_attendance::storage::ScheduledShift;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

struct FakeScheduleRepository final : IScheduleRepository {
    Result<std::optional<ScheduledShift>, RepositoryError> result =
        Result<std::optional<ScheduledShift>, RepositoryError>::success(
            ScheduledShift{7, {"09:00", "12:00"}, {"13:00", "18:00"}});
    int calls{0};
    bool shouldThrow{false};

    Result<std::optional<ScheduledShift>, RepositoryError>
    findForUserAt(int, std::int64_t) override {
        ++calls;
        if (shouldThrow) {
            throw std::runtime_error("schedule read failure");
        }
        return result;
    }
};

struct FakeRuleRepository final : IAttendanceRuleRepository {
    Result<AttendanceRules, RepositoryError> result =
        Result<AttendanceRules, RepositoryError>::success({15, 5});
    int calls{0};
    bool shouldThrow{false};

    Result<AttendanceRules, RepositoryError> load() override {
        ++calls;
        if (shouldThrow) {
            throw std::runtime_error("rule read failure");
        }
        return result;
    }
};

struct FakeAttendanceRepository final : IAttendanceRepository {
    Result<bool, RepositoryError> duplicateResult =
        Result<bool, RepositoryError>::success(false);
    Result<void, RepositoryError> saveResult =
        Result<void, RepositoryError>::success();
    int duplicateCalls{0};
    int saveCalls{0};
    std::int64_t rangeStart{-1};
    std::optional<AttendanceEntry> savedEntry;
    bool duplicateShouldThrow{false};
    bool saveShouldThrow{false};

    Result<bool, RepositoryError> hasRecordInRange(
        int, std::int64_t startTimestamp, std::int64_t) override {
        ++duplicateCalls;
        if (duplicateShouldThrow) {
            throw std::runtime_error("attendance read failure");
        }
        rangeStart = startTimestamp;
        return duplicateResult;
    }

    Result<void, RepositoryError> save(const AttendanceEntry& entry) override {
        ++saveCalls;
        if (saveShouldThrow) {
            throw std::runtime_error("attendance write failure");
        }
        savedEntry = entry;
        return saveResult;
    }
};

void testSuccessfulPunch() {
    FakeScheduleRepository schedules;
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    const auto result = service.punch({42, 1000, 9 * 60 + 10});

    require(result.hasValue(), "valid punch should succeed");
    require(result.value().shiftId == 7, "receipt should expose selected shift");
    require(result.value().status == PunchStatus::Late,
            "09:10 should be late within the threshold");
    require(result.value().minutesDifference == 10,
            "late minutes should be preserved");
    require(result.value().checkIn, "morning punch should be check-in");
    require(attendance.rangeStart == 700,
            "duplicate window should use the configured five minutes");
    require(attendance.saveCalls == 1 && attendance.savedEntry.has_value(),
            "successful punch should persist exactly once");
    require(attendance.savedEntry->timestamp == 1000,
            "repository should receive the request timestamp");
}

void testDuplicateDoesNotWrite() {
    FakeScheduleRepository schedules;
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    attendance.duplicateResult = Result<bool, RepositoryError>::success(true);
    PunchService service(schedules, rules, attendance);

    const auto result = service.punch({42, 1000, 9 * 60});

    require(!result && result.error() == PunchError::DuplicatePunch,
            "duplicate punch should return a business error");
    require(attendance.saveCalls == 0, "duplicate punch must not be persisted");
}

void testNoShiftStopsChain() {
    FakeScheduleRepository schedules;
    schedules.result =
        Result<std::optional<ScheduledShift>, RepositoryError>::success(std::nullopt);
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    const auto result = service.punch({42, 1000, 9 * 60});

    require(!result && result.error() == PunchError::NoShift,
            "missing shift should be a non-storage business failure");
    require(rules.calls == 0 && attendance.duplicateCalls == 0 &&
                attendance.saveCalls == 0,
            "no-shift result should stop remaining work");
}

void testRepositoryErrorsAreMapped() {
    FakeScheduleRepository schedules;
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    schedules.result =
        Result<std::optional<ScheduledShift>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    auto result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::ScheduleReadFailed,
            "schedule error should retain its operation context");

    schedules.result =
        Result<std::optional<ScheduledShift>, RepositoryError>::success(
            ScheduledShift{7, {"09:00", "12:00"}, {"13:00", "18:00"}});
    rules.result = Result<AttendanceRules, RepositoryError>::failure(
        RepositoryError::ReadFailed);
    result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::RulesReadFailed,
            "rule read error should retain its operation context");

    rules.result = Result<AttendanceRules, RepositoryError>::success({15, 5});
    attendance.duplicateResult =
        Result<bool, RepositoryError>::failure(RepositoryError::ReadFailed);
    result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::AttendanceReadFailed,
            "duplicate lookup error should not be reported as duplicate");

    attendance.duplicateResult = Result<bool, RepositoryError>::success(false);
    attendance.saveResult =
        Result<void, RepositoryError>::failure(RepositoryError::WriteFailed);
    result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::WriteFailed,
            "write error should not report a successful punch");
}

void testRepositoryExceptionsAreMapped() {
    FakeScheduleRepository schedules;
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    schedules.shouldThrow = true;
    auto result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::ScheduleReadFailed,
            "schedule exception must not cross the service boundary");

    schedules.shouldThrow = false;
    rules.shouldThrow = true;
    result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::RulesReadFailed,
            "rule exception must not cross the service boundary");

    rules.shouldThrow = false;
    attendance.duplicateShouldThrow = true;
    result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::AttendanceReadFailed,
            "attendance lookup exception must not cross the service boundary");

    attendance.duplicateShouldThrow = false;
    attendance.saveShouldThrow = true;
    result = service.punch({42, 1000, 9 * 60});
    require(!result && result.error() == PunchError::WriteFailed,
            "attendance write exception must not cross the service boundary");
}

void testInvalidShiftDoesNotWrite() {
    FakeScheduleRepository schedules;
    schedules.result =
        Result<std::optional<ScheduledShift>, RepositoryError>::success(
            ScheduledShift{7, {"invalid", "12:00"}, {"13:00", "18:00"}});
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    const auto result = service.punch({42, 1000, 9 * 60});

    require(!result && result.error() == PunchError::InvalidShift,
            "invalid schedule must be observable");
    require(attendance.saveCalls == 0, "invalid schedule must not be persisted");
}

void testInvalidRulesDoNotAccessAttendance() {
    FakeScheduleRepository schedules;
    FakeRuleRepository rules;
    rules.result = Result<AttendanceRules, RepositoryError>::success({-1, 5});
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    const auto result = service.punch({42, 1000, 9 * 60});

    require(!result && result.error() == PunchError::InvalidRules,
            "invalid rule configuration must be distinguishable from a bad shift");
    require(attendance.duplicateCalls == 0 && attendance.saveCalls == 0,
            "invalid rules must stop before attendance access");
}

void testInvalidRequestStopsChain() {
    FakeScheduleRepository schedules;
    FakeRuleRepository rules;
    FakeAttendanceRepository attendance;
    PunchService service(schedules, rules, attendance);

    const auto result = service.punch({0, 1000, 9 * 60});

    require(!result && result.error() == PunchError::InvalidRequest,
            "non-positive user id should be rejected");
    require(schedules.calls == 0 && rules.calls == 0 &&
                attendance.duplicateCalls == 0 && attendance.saveCalls == 0,
            "invalid request should not access repositories");
}

void testPurePunchStatuses() {
    const ShiftPeriod first{"09:00", "12:00"};
    const ShiftPeriod second{"13:00", "18:00"};

    const auto normal = calculatePunch(8 * 60 + 50, first, second, 15);
    require(normal && normal->status == PunchStatus::Normal && normal->checkIn,
            "early arrival should be a normal check-in");

    const auto absent = calculatePunch(9 * 60 + 20, first, second, 15);
    require(absent && absent->status == PunchStatus::Absent &&
                absent->minutesDifference == 20,
            "arrival beyond threshold should be absent");

    const auto early = calculatePunch(17 * 60 + 30, first, second, 15);
    require(early && early->status == PunchStatus::Early && !early->checkIn &&
                early->minutesDifference == 30,
            "departure before shift end should be early");

    const auto checkout = calculatePunch(18 * 60 + 10, first, second, 15);
    require(checkout && checkout->status == PunchStatus::Normal &&
                !checkout->checkIn,
            "departure after shift end should be normal");
}

} // namespace

int main() {
    testSuccessfulPunch();
    testDuplicateDoesNotWrite();
    testNoShiftStopsChain();
    testRepositoryErrorsAreMapped();
    testRepositoryExceptionsAreMapped();
    testInvalidShiftDoesNotWrite();
    testInvalidRulesDoNotAccessAttendance();
    testInvalidRequestStopsChain();
    testPurePunchStatuses();
    std::cout << "punch_service_test: PASS\n";
    return 0;
}

#include "business/attendance_rule.h"
#include "core/attendance/punch_rule.h"

#include <ctime>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void requireMinutes(const std::string& text, int expected) {
    const std::optional<int> coreResult =
        smart_attendance::core::parseFlexibleTimeToMinutes(text);
    require(coreResult && *coreResult == expected,
            "core flexible parser returned unexpected minutes");
    require(AttendanceRule::timeStringToMinutes(text) == expected,
            "legacy adapter differs from core flexible parser");
}

void requireInvalid(const std::string& text) {
    require(!smart_attendance::core::parseFlexibleTimeToMinutes(text),
            "core flexible parser accepted invalid input");
    require(AttendanceRule::timeStringToMinutes(text) == -1,
            "legacy adapter did not preserve the invalid-input sentinel");
}

void testSupportedLegacyFormats() {
    requireMinutes("09:00", 9 * 60);
    requireMinutes(" 09:00 ", 9 * 60);
    requireMinutes("9：00", 9 * 60);
    requireMinutes("09。00", 9 * 60);
    requireMinutes("09·00", 9 * 60);
    requireMinutes("#09:00.", 9 * 60);
    requireMinutes("09.00", 9 * 60);
    requireMinutes("09-00", 9 * 60);
    requireMinutes("9 00", 9 * 60);
    requireMinutes("0900", 9 * 60);
    requireMinutes("900", 9 * 60);
    requireMinutes("9", 9 * 60);
}

void testInvalidInputs() {
    requireInvalid("");
    requireInvalid("not-a-time");
    requireInvalid("24:00");
    requireInvalid("09:60");
    requireInvalid("09:-1");
    requireInvalid("12345");
    requireInvalid(std::string(1000, '9') + ":00");
}

std::time_t makeLocalTimestamp(int hour, int minute) {
    std::tm localTime{};
    localTime.tm_year = 124;
    localTime.tm_mon = 0;
    localTime.tm_mday = 15;
    localTime.tm_hour = hour;
    localTime.tm_min = minute;
    localTime.tm_isdst = -1;
    const std::time_t timestamp = std::mktime(&localTime);
    require(timestamp != static_cast<std::time_t>(-1),
            "failed to create local test timestamp");
    return timestamp;
}

void requireOwner(int localMinute,
                  int firstEndMinute,
                  int secondStartMinute,
                  smart_attendance::core::ShiftOwner expected,
                  const ShiftConfig& first,
                  const ShiftConfig& second) {
    require(smart_attendance::core::determineShiftOwner(
                localMinute, firstEndMinute, secondStartMinute) == expected,
            "core shift ownership differs from expected result");
    const int legacyExpected =
        expected == smart_attendance::core::ShiftOwner::FirstPeriod ? 1 : 2;
    require(AttendanceRule::determineShiftOwner(
                makeLocalTimestamp(localMinute / 60, localMinute % 60),
                first,
                second) == legacyExpected,
            "legacy shift ownership adapter differs from core result");
}

void testShiftOwnershipCompatibility() {
    const ShiftConfig first{"09:00", "12:00", 15};
    const ShiftConfig second{"13:00", "18:00", 15};
    requireOwner(12 * 60, 12 * 60, 13 * 60,
                 smart_attendance::core::ShiftOwner::FirstPeriod,
                 first, second);
    requireOwner(12 * 60 + 30, 12 * 60, 13 * 60,
                 smart_attendance::core::ShiftOwner::FirstPeriod,
                 first, second);
    requireOwner(12 * 60 + 31, 12 * 60, 13 * 60,
                 smart_attendance::core::ShiftOwner::SecondPeriod,
                 first, second);
    requireOwner(13 * 60, 12 * 60, 13 * 60,
                 smart_attendance::core::ShiftOwner::SecondPeriod,
                 first, second);
}

void testCrossMidnightShiftOwnership() {
    const ShiftConfig first{"18:00", "23:00", 15};
    const ShiftConfig second{"00:00", "06:00", 15};
    requireOwner(22 * 60 + 30, 23 * 60, 0,
                 smart_attendance::core::ShiftOwner::FirstPeriod,
                 first, second);
    requireOwner(60, 23 * 60, 0,
                 smart_attendance::core::ShiftOwner::SecondPeriod,
                 first, second);
}

PunchStatus toLegacyStatus(smart_attendance::core::PunchStatus status) {
    switch (status) {
        case smart_attendance::core::PunchStatus::Normal:
            return PunchStatus::NORMAL;
        case smart_attendance::core::PunchStatus::Late:
            return PunchStatus::LATE;
        case smart_attendance::core::PunchStatus::Early:
            return PunchStatus::EARLY;
        case smart_attendance::core::PunchStatus::Absent:
            return PunchStatus::ABSENT;
    }
    return PunchStatus::NORMAL;
}

void requireStatus(int localMinute,
                   const ShiftConfig& shift,
                   bool checkIn,
                   smart_attendance::core::PunchStatus expectedStatus,
                   int expectedDifference) {
    const int startMinute = AttendanceRule::timeStringToMinutes(shift.start_time);
    const int endMinute = AttendanceRule::timeStringToMinutes(shift.end_time);
    const auto coreResult = smart_attendance::core::calculatePunchStatus(
        localMinute,
        startMinute,
        endMinute,
        shift.late_threshold_min,
        checkIn);
    require(coreResult.status == expectedStatus &&
                coreResult.minutesDifference == expectedDifference,
            "core punch status differs from expected result");

    const PunchResult legacyResult = AttendanceRule::calculatePunchStatus(
        makeLocalTimestamp(localMinute / 60, localMinute % 60), shift, checkIn);
    require(legacyResult.status == toLegacyStatus(expectedStatus) &&
                legacyResult.minutes_diff == expectedDifference,
            "legacy punch status adapter differs from core result");
}

void testPunchStatusCompatibility() {
    const ShiftConfig dayShift{"09:00", "18:00", 15};
    requireStatus(8 * 60 + 50, dayShift, true,
                  smart_attendance::core::PunchStatus::Normal, 0);
    requireStatus(9 * 60 + 15, dayShift, true,
                  smart_attendance::core::PunchStatus::Late, 15);
    requireStatus(9 * 60 + 16, dayShift, true,
                  smart_attendance::core::PunchStatus::Absent, 16);
    requireStatus(17 * 60 + 30, dayShift, false,
                  smart_attendance::core::PunchStatus::Early, 30);
    requireStatus(18 * 60, dayShift, false,
                  smart_attendance::core::PunchStatus::Normal, 0);
}

void testCrossMidnightPunchStatus() {
    const ShiftConfig nightShift{"22:00", "06:00", 15};
    requireStatus(60, nightShift, false,
                  smart_attendance::core::PunchStatus::Early, 5 * 60);
    requireStatus(6 * 60, nightShift, false,
                  smart_attendance::core::PunchStatus::Normal, 0);
}

void testStatusPriorityCompatibility() {
    using CoreStatus = smart_attendance::core::PunchStatus;
    require(smart_attendance::core::isPunchStatusBetter(
                CoreStatus::Normal, CoreStatus::Late),
            "normal status should outrank late status");
    require(smart_attendance::core::isPunchStatusBetter(
                CoreStatus::Late, CoreStatus::Early),
            "late status should retain its legacy priority over early status");
    require(!smart_attendance::core::isPunchStatusBetter(
                CoreStatus::Absent, CoreStatus::Normal),
            "absent status must not replace normal status");
    require(!smart_attendance::core::isPunchStatusBetter(
                CoreStatus::Late, CoreStatus::Late),
            "equal statuses should defer to the report timestamp rule");

    require(AttendanceRule::isStatusBetter(0, 3),
            "legacy status adapter should use core priority for valid codes");
    require(!AttendanceRule::isStatusBetter(2, 1),
            "legacy status adapter reversed valid status priority");
    require(AttendanceRule::isStatusBetter(-1, 0),
            "legacy negative sentinel comparison changed");
    require(!AttendanceRule::isStatusBetter(4, 3),
            "legacy unknown status comparison changed");
}

} // namespace

int main() {
    testSupportedLegacyFormats();
    testInvalidInputs();
    testShiftOwnershipCompatibility();
    testCrossMidnightShiftOwnership();
    testPunchStatusCompatibility();
    testCrossMidnightPunchStatus();
    testStatusPriorityCompatibility();
    std::cout << "attendance_rule_compat_test: PASS\n";
    return 0;
}

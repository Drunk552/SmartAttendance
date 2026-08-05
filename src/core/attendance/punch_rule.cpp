/**
 * @file punch_rule.cpp
 * @brief 实现打卡归属与状态计算的纯业务规则。
 */

#include "punch_rule.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace smart_attendance::core {
namespace {

std::optional<int> normalizedEndMinute(const ShiftPeriod& period) noexcept {
    const auto start = parseTimeToMinutes(period.startTime);
    const auto end = parseTimeToMinutes(period.endTime);
    if (!start || !end) {
        return std::nullopt;
    }
    return *end < *start ? *end + 1440 : *end;
}

int statusPriority(PunchStatus status) noexcept {
    switch (status) {
        case PunchStatus::Normal:
            return 0;
        case PunchStatus::Late:
            return 1;
        case PunchStatus::Early:
            return 2;
        case PunchStatus::Absent:
            return 3;
    }
    return 3;
}

} // namespace

bool isPunchStatusBetter(PunchStatus candidate, PunchStatus current) noexcept {
    return statusPriority(candidate) < statusPriority(current);
}

std::optional<int> parseTimeToMinutes(const std::string& value) noexcept {
    if (value.size() != 5 || value[2] != ':' ||
        !std::isdigit(static_cast<unsigned char>(value[0])) ||
        !std::isdigit(static_cast<unsigned char>(value[1])) ||
        !std::isdigit(static_cast<unsigned char>(value[3])) ||
        !std::isdigit(static_cast<unsigned char>(value[4]))) {
        return std::nullopt;
    }

    const int hour = (value[0] - '0') * 10 + (value[1] - '0');
    const int minute = (value[3] - '0') * 10 + (value[4] - '0');
    if (hour > 23 || minute > 59) {
        return std::nullopt;
    }
    return hour * 60 + minute;
}

std::optional<int> parseFlexibleTimeToMinutes(const std::string& value) noexcept {
    if (value.empty()) {
        return std::nullopt;
    }

    std::string text = value;
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base(), text.end());
    if (text.empty()) {
        return std::nullopt;
    }

    std::string cleaned;
    cleaned.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto current = static_cast<unsigned char>(text[i]);
        if (i + 2 < text.size() && current == 0xEF &&
            static_cast<unsigned char>(text[i + 1]) == 0xBC &&
            static_cast<unsigned char>(text[i + 2]) == 0x9A) {
            cleaned.push_back(':');
            i += 3;
        } else if (i + 2 < text.size() && current == 0xE3 &&
                   static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                   static_cast<unsigned char>(text[i + 2]) == 0x82) {
            cleaned.push_back(':');
            i += 3;
        } else if (i + 1 < text.size() && current == 0xC2 &&
                   static_cast<unsigned char>(text[i + 1]) == 0xB7) {
            cleaned.push_back(':');
            i += 2;
        } else {
            cleaned.push_back(text[i]);
            ++i;
        }
    }

    const std::size_t firstDigit = cleaned.find_first_of("0123456789");
    const std::size_t lastDigit = cleaned.find_last_of("0123456789");
    if (firstDigit == std::string::npos) {
        return std::nullopt;
    }
    text = cleaned.substr(firstDigit, lastDigit - firstDigit + 1);

    for (char& character : text) {
        if (character == '.' || character == '-' || character == ' ') {
            character = ':';
        }
    }

    if (text.find(':') == std::string::npos) {
        if (!std::all_of(text.begin(), text.end(), [](unsigned char c) {
                return std::isdigit(c);
            })) {
            return std::nullopt;
        }
        if (text.size() == 4) {
            text.insert(2, 1, ':');
        } else if (text.size() == 3) {
            text.insert(1, 1, ':');
            text.insert(0, 1, '0');
        } else if (text.size() <= 2) {
            text += ":00";
        } else {
            return std::nullopt;
        }
    }

    const std::size_t separator = text.find(':');
    std::string hourText = text.substr(0, separator);
    std::string minuteText = text.substr(separator + 1);
    const auto removeWhitespace = [](std::string& part) {
        part.erase(std::remove_if(part.begin(), part.end(), [](unsigned char c) {
            return std::isspace(c);
        }), part.end());
    };
    removeWhitespace(hourText);
    removeWhitespace(minuteText);

    const auto parseNumber = [](const std::string& part, int maximum)
        -> std::optional<int> {
        if (part.empty()) {
            return std::nullopt;
        }
        int result = 0;
        for (const unsigned char character : part) {
            if (!std::isdigit(character)) {
                return std::nullopt;
            }
            const int digit = character - '0';
            if (result > (maximum - digit) / 10) {
                return std::nullopt;
            }
            result = result * 10 + digit;
        }
        return result;
    };

    const auto hour = parseNumber(hourText, 23);
    const auto minute = parseNumber(minuteText, 59);
    if (!hour || !minute) {
        return std::nullopt;
    }
    return *hour * 60 + *minute;
}

ShiftOwner determineShiftOwner(int localMinute,
                               int firstEndMinute,
                               int secondStartMinute) noexcept {
    std::int64_t comparableMinute = localMinute;
    const std::int64_t comparableFirstEnd = firstEndMinute;
    std::int64_t comparableSecondStart = secondStartMinute;
    if (comparableSecondStart < comparableFirstEnd) {
        comparableSecondStart += 1440;
    }
    if (comparableFirstEnd > 1000 && comparableMinute < 480) {
        comparableMinute += 1440;
    }

    if (comparableMinute <= comparableFirstEnd) {
        return ShiftOwner::FirstPeriod;
    }
    if (comparableMinute >= comparableSecondStart) {
        return ShiftOwner::SecondPeriod;
    }

    const std::int64_t midpoint =
        comparableFirstEnd + (comparableSecondStart - comparableFirstEnd) / 2;
    return comparableMinute <= midpoint ? ShiftOwner::FirstPeriod
                                        : ShiftOwner::SecondPeriod;
}

PunchStatusCalculation calculatePunchStatus(int localMinute,
                                             int shiftStartMinute,
                                             int shiftEndMinute,
                                             int lateThresholdMinutes,
                                             bool checkIn) noexcept {
    const std::int64_t startMinute = shiftStartMinute;
    std::int64_t endMinute = shiftEndMinute;
    if (endMinute < startMinute) {
        endMinute += 1440;
    }

    std::int64_t comparableMinute = localMinute;
    if (startMinute > 1080 && comparableMinute < 720) {
        comparableMinute += 1440;
    }

    PunchStatusCalculation result{PunchStatus::Normal, 0};
    if (checkIn) {
        if (comparableMinute <= startMinute) {
            return result;
        }

        const std::int64_t lateMinutes = comparableMinute - startMinute;
        result.minutesDifference = static_cast<int>(lateMinutes);
        result.status = lateMinutes <= static_cast<std::int64_t>(lateThresholdMinutes)
                            ? PunchStatus::Late
                            : PunchStatus::Absent;
        return result;
    }

    if (comparableMinute < endMinute) {
        result.status = PunchStatus::Early;
        result.minutesDifference = static_cast<int>(endMinute - comparableMinute);
    }
    return result;
}

std::optional<PunchCalculation> calculatePunch(
    int localMinute,
    const ShiftPeriod& firstPeriod,
    const ShiftPeriod& secondPeriod,
    int lateThresholdMinutes) noexcept {
    if (localMinute < 0 || localMinute >= 1440 || lateThresholdMinutes < 0) {
        return std::nullopt;
    }

    const auto firstStart = parseTimeToMinutes(firstPeriod.startTime);
    const auto firstEnd = normalizedEndMinute(firstPeriod);
    const auto secondStart = parseTimeToMinutes(secondPeriod.startTime);
    const auto secondEnd = normalizedEndMinute(secondPeriod);
    if (!firstStart || !firstEnd || !secondStart || !secondEnd) {
        return std::nullopt;
    }

    const bool checkIn = determineShiftOwner(
                             localMinute, *firstEnd, *secondStart) ==
                         ShiftOwner::FirstPeriod;

    const ShiftPeriod& target = checkIn ? firstPeriod : secondPeriod;
    const int targetStart = *parseTimeToMinutes(target.startTime);
    const int targetEnd = *parseTimeToMinutes(target.endTime);
    const PunchStatusCalculation status = calculatePunchStatus(
        localMinute,
        targetStart,
        targetEnd,
        lateThresholdMinutes,
        checkIn);
    return PunchCalculation{status.status, status.minutesDifference, checkIn};
}

} // namespace smart_attendance::core

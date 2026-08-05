/**
 * @file punch_rule.h
 * @brief 声明不依赖 UI、数据库和图像库的打卡领域规则。
 */

#ifndef SMART_ATTENDANCE_CORE_ATTENDANCE_PUNCH_RULE_H
#define SMART_ATTENDANCE_CORE_ATTENDANCE_PUNCH_RULE_H

#include <optional>
#include <string>

namespace smart_attendance::core {

enum class PunchStatus {
    Normal,
    Late,
    Early,
    Absent
};

enum class ShiftOwner {
    FirstPeriod,
    SecondPeriod
};

struct ShiftPeriod {
    std::string startTime;
    std::string endTime;
};

struct PunchStatusCalculation {
    PunchStatus status;
    int minutesDifference;
};

struct PunchCalculation {
    PunchStatus status;
    int minutesDifference;
    bool checkIn;
};

/** @brief 判断候选状态是否比当前状态更优。相同状态返回 false。 */
bool isPunchStatusBetter(PunchStatus candidate, PunchStatus current) noexcept;

/** @brief 严格解析 HH:MM；无效输入返回空值。 */
std::optional<int> parseTimeToMinutes(const std::string& value) noexcept;

/**
 * @brief 宽容解析 UI 和旧配置中的时间文本。
 *
 * 支持首尾 ASCII 空白、全角冒号、常见分隔符以及 1 至 4 位纯数字格式。
 * 解析不访问外部资源，不抛出异常；无效输入返回空值。
 */
std::optional<int> parseFlexibleTimeToMinutes(const std::string& value) noexcept;

/**
 * @brief 按两个班段的间隔中点判断打卡归属。
 *
 * 分钟值为已解析的当天偏移；函数内部兼容第二班段跨天的线性时间轴。
 * 该纯函数不读取系统时钟，也不访问外部资源。
 */
ShiftOwner determineShiftOwner(int localMinute,
                               int firstEndMinute,
                               int secondStartMinute) noexcept;

/**
 * @brief 根据已解析的班段分钟计算单次打卡状态。
 *
 * 下班时间早于上班时间时按跨天班次处理。调用方负责传入已解析
 * 的分钟值与配置阈值；函数不读取时钟或外部资源。
 */
PunchStatusCalculation calculatePunchStatus(int localMinute,
                                             int shiftStartMinute,
                                             int shiftEndMinute,
                                             int lateThresholdMinutes,
                                             bool checkIn) noexcept;

/**
 * @brief 根据当前本地分钟和两个班段计算打卡状态。
 * @return 班段无效时返回空值；该纯函数不访问时钟或外部资源。
 */
std::optional<PunchCalculation> calculatePunch(
    int localMinute,
    const ShiftPeriod& firstPeriod,
    const ShiftPeriod& secondPeriod,
    int lateThresholdMinutes) noexcept;

} // namespace smart_attendance::core

#endif // SMART_ATTENDANCE_CORE_ATTENDANCE_PUNCH_RULE_H

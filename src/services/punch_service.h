/**
 * @file punch_service.h
 * @brief 声明统一认证方式共用的打卡业务编排服务。
 */

#ifndef SMART_ATTENDANCE_SERVICES_PUNCH_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_PUNCH_SERVICE_H

#include "core/common/result.h"
#include "storage/repository/punch_repositories.h"

#include <cstdint>
#include <vector>

namespace smart_attendance::services {

enum class PunchError {
    InvalidRequest,
    ScheduleReadFailed,
    NoShift,
    RulesReadFailed,
    AttendanceReadFailed,
    DuplicatePunch,
    InvalidRules,
    InvalidShift,
    WriteFailed
};

struct PunchRequest {
    int userId;
    std::int64_t timestamp;
    int localMinute;
    /** @brief 可选的 JPEG 抓拍数据；所有权随请求转移到打卡 Worker。 */
    std::vector<std::uint8_t> snapshotJpeg{};
};

struct PunchReceipt {
    int shiftId;
    core::PunchStatus status;
    int minutesDifference;
    bool checkIn;
};

/**
 * @brief 编排排班、重复检查、状态计算和考勤记录写入。
 *
 * 本类不拥有 Repository，不创建线程，也不访问 UI、SQLite 或 OpenCV。
 * 调用方负责保证三个 Repository 在 PunchService 生命周期内保持有效。
 */
class PunchService final {
public:
    PunchService(storage::IScheduleRepository& scheduleRepository,
                 storage::IAttendanceRuleRepository& ruleRepository,
                 storage::IAttendanceRepository& attendanceRepository) noexcept;

    Result<PunchReceipt, PunchError> punch(PunchRequest request);

private:
    storage::IScheduleRepository& scheduleRepository_;
    storage::IAttendanceRuleRepository& ruleRepository_;
    storage::IAttendanceRepository& attendanceRepository_;
};

} // namespace smart_attendance::services

#endif // SMART_ATTENDANCE_SERVICES_PUNCH_SERVICE_H

#ifndef SMART_ATTENDANCE_SERVICES_ATTENDANCE_QUERY_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_ATTENDANCE_QUERY_SERVICE_H

#include "storage/repository/attendance_query_repository.h"

#include <cstdint>
#include <vector>

namespace smart_attendance::services {
enum class AttendanceQueryError { InvalidRange, ReadFailed };
constexpr std::size_t kMaxAttendanceQuerySize = 512;
constexpr std::size_t kAttendancePageSize = 20;
struct AttendancePage {
    std::vector<core::AttendanceRecord> records;
    std::size_t pageIndex{0};
    bool hasPrevious{false};
    bool hasNext{false};
};
class AttendanceQueryService final {
public:
    explicit AttendanceQueryService(storage::IAttendanceQueryRepository& repository) noexcept : repository_(repository) {}
    Result<std::vector<core::AttendanceRecord>, AttendanceQueryError> query(
        int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp);
    Result<AttendancePage, AttendanceQueryError> queryPage(
        int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp,
        std::size_t pageIndex);
private:
    storage::IAttendanceQueryRepository& repository_;
};
}
#endif

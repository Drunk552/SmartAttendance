#include "attendance_query_service.h"

namespace smart_attendance::services {
Result<std::vector<core::AttendanceRecord>, AttendanceQueryError>
AttendanceQueryService::query(int employeeId, std::int64_t startTimestamp,
                              std::int64_t endTimestamp) {
    using Return = Result<std::vector<core::AttendanceRecord>, AttendanceQueryError>;
    if (startTimestamp > endTimestamp) return Return::failure(AttendanceQueryError::InvalidRange);
    const auto result = repository_.query(
        employeeId, startTimestamp, endTimestamp, kMaxAttendanceQuerySize, 0);
    if (!result) {
        return Return::failure(result.error() == storage::RepositoryError::InvalidArgument
                                   ? AttendanceQueryError::InvalidRange
                                   : AttendanceQueryError::ReadFailed);
    }
    return Return::success(result.value().records);
}

Result<AttendancePage, AttendanceQueryError> AttendanceQueryService::queryPage(
    int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp,
    std::size_t pageIndex) {
    using Return = Result<AttendancePage, AttendanceQueryError>;
    if (startTimestamp > endTimestamp) return Return::failure(AttendanceQueryError::InvalidRange);
    const auto result = repository_.query(employeeId, startTimestamp, endTimestamp,
                                          kAttendancePageSize,
                                          pageIndex * kAttendancePageSize);
    if (!result) return Return::failure(AttendanceQueryError::ReadFailed);
    return Return::success({result.value().records, pageIndex, pageIndex > 0,
                            result.value().hasMore});
}
}

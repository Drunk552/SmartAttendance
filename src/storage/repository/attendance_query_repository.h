#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_ATTENDANCE_QUERY_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_ATTENDANCE_QUERY_REPOSITORY_H

#include "core/common/result.h"
#include "core/model/attendance_record.h"
#include "storage/repository/repository_error.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace smart_attendance::storage {
struct AttendanceQueryPage {
    std::vector<core::AttendanceRecord> records;
    bool hasMore{false};
};
class IAttendanceQueryRepository {
public:
    virtual ~IAttendanceQueryRepository() = default;
    virtual Result<AttendanceQueryPage, RepositoryError> query(
        int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp,
        std::size_t limit, std::size_t offset) = 0;
};
}
#endif

#ifndef SMART_ATTENDANCE_UI_PRESENTERS_ATTENDANCE_QUERY_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_ATTENDANCE_QUERY_PRESENTER_H

#include "services/attendance_query_service.h"

#include <cstdint>
#include <string>
#include <vector>

namespace smart_attendance::ui {
struct AttendanceRecordItem {
    int id{0};
    int employeeId{0};
    std::string employeeName;
    std::string departmentName;
    std::int64_t timestamp{0};
    int status{0};
    std::string imagePath;
};
struct AttendanceRecordPage {
    std::vector<AttendanceRecordItem> records;
    std::size_t pageIndex{0};
    bool hasPrevious{false};
    bool hasNext{false};
};
class AttendanceQueryPresenter final {
public:
    explicit AttendanceQueryPresenter(services::AttendanceQueryService& service) noexcept : service_(service) {}
    std::vector<AttendanceRecordItem> query(int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp);
    AttendanceRecordPage queryPage(int employeeId, std::int64_t startTimestamp,
                                   std::int64_t endTimestamp, std::size_t pageIndex);
private:
    services::AttendanceQueryService& service_;
};
}
#endif

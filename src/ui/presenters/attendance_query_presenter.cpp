#include "attendance_query_presenter.h"

namespace smart_attendance::ui {
std::vector<AttendanceRecordItem> AttendanceQueryPresenter::query(
    int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp) {
    const auto result = service_.query(employeeId, startTimestamp, endTimestamp);
    if (!result) return {};
    std::vector<AttendanceRecordItem> items;
    items.reserve(result.value().size());
    for (const auto& record : result.value()) {
        items.push_back({record.id, record.employeeId, record.employeeName,
                         record.departmentName, record.timestamp, record.status,
                         record.imagePath});
    }
    return items;
}

AttendanceRecordPage AttendanceQueryPresenter::queryPage(
    int employeeId, std::int64_t startTimestamp, std::int64_t endTimestamp,
    std::size_t pageIndex) {
    const auto result = service_.queryPage(employeeId, startTimestamp,
                                            endTimestamp, pageIndex);
    if (!result) return {};
    AttendanceRecordPage page;
    page.pageIndex = result.value().pageIndex;
    page.hasPrevious = result.value().hasPrevious;
    page.hasNext = result.value().hasNext;
    page.records.reserve(result.value().records.size());
    for (const auto& record : result.value().records) {
        page.records.push_back({record.id, record.employeeId, record.employeeName,
                                record.departmentName, record.timestamp,
                                record.status, record.imagePath});
    }
    return page;
}
}

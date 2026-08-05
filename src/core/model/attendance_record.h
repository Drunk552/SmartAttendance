#ifndef SMART_ATTENDANCE_CORE_MODEL_ATTENDANCE_RECORD_H
#define SMART_ATTENDANCE_CORE_MODEL_ATTENDANCE_RECORD_H

#include <cstdint>
#include <string>

namespace smart_attendance::core {
struct AttendanceRecord {
    int id{0};
    int employeeId{0};
    std::string employeeName;
    std::string departmentName;
    std::int64_t timestamp{0};
    int status{0};
    std::string imagePath;
};
}
#endif

#ifndef SMART_ATTENDANCE_CORE_MODEL_SHIFT_H
#define SMART_ATTENDANCE_CORE_MODEL_SHIFT_H

#include <string>

namespace smart_attendance::core {

struct Shift {
    int id{0};
    std::string name;
    std::string firstStart;
    std::string firstEnd;
    std::string secondStart;
    std::string secondEnd;
    std::string thirdStart;
    std::string thirdEnd;
    int crossDay{0};
};

} // namespace smart_attendance::core

#endif

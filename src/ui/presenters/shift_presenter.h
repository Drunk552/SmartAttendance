#ifndef SMART_ATTENDANCE_UI_PRESENTERS_SHIFT_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_SHIFT_PRESENTER_H

#include "services/shift_service.h"

#include <string>
#include <vector>

namespace smart_attendance::ui {
struct ShiftItem {
    int id{0};
    std::string name;
    std::string firstStart, firstEnd, secondStart, secondEnd, thirdStart, thirdEnd;
    int crossDay{0};
};

class ShiftPresenter final {
public:
    explicit ShiftPresenter(services::ShiftService& service) noexcept : service_(service) {}
    std::vector<ShiftItem> listAll();
    bool findById(int shiftId, ShiftItem& item);
    bool update(const ShiftItem& item);
private:
    services::ShiftService& service_;
};
} // namespace smart_attendance::ui
#endif

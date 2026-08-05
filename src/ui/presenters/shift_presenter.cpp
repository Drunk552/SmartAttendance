#include "shift_presenter.h"

namespace smart_attendance::ui {
namespace {
ShiftItem mapItem(const core::Shift& value) {
    return {value.id, value.name, value.firstStart, value.firstEnd, value.secondStart,
            value.secondEnd, value.thirdStart, value.thirdEnd, value.crossDay};
}
core::Shift mapShift(const ShiftItem& value) {
    return {value.id, value.name, value.firstStart, value.firstEnd, value.secondStart,
            value.secondEnd, value.thirdStart, value.thirdEnd, value.crossDay};
}
}
std::vector<ShiftItem> ShiftPresenter::listAll() {
    const auto result = service_.listShifts();
    if (!result) return {};
    std::vector<ShiftItem> items;
    for (const auto& shift : result.value()) items.push_back(mapItem(shift));
    return items;
}
bool ShiftPresenter::findById(int shiftId, ShiftItem& item) {
    const auto result = service_.findById(shiftId);
    if (!result) return false;
    item = mapItem(result.value());
    return true;
}
bool ShiftPresenter::update(const ShiftItem& item) {
    return static_cast<bool>(service_.update(mapShift(item)));
}
} // namespace smart_attendance::ui

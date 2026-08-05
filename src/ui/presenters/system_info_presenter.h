#ifndef SMART_ATTENDANCE_UI_PRESENTERS_SYSTEM_INFO_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_SYSTEM_INFO_PRESENTER_H

#include "services/system_info_service.h"

namespace smart_attendance::ui {

class SystemInfoPresenter final {
public:
    explicit SystemInfoPresenter(services::SystemInfoService& service) noexcept
        : service_(service) {}
    bool statistics(SystemStats& stats) {
        const auto result = service_.statistics();
        if (!result) return false;
        stats = result.value();
        return true;
    }
private:
    services::SystemInfoService& service_;
};

} // namespace smart_attendance::ui

#endif

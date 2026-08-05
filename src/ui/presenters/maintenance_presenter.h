#ifndef SMART_ATTENDANCE_UI_PRESENTERS_MAINTENANCE_PRESENTER_H
#define SMART_ATTENDANCE_UI_PRESENTERS_MAINTENANCE_PRESENTER_H

#include "services/maintenance_service.h"

namespace smart_attendance::ui {
class MaintenancePresenter final {
public:
    explicit MaintenancePresenter(services::MaintenanceService& service) noexcept : service_(service) {}
    bool clearAttendance();
    bool clearEmployees();
    bool clearAllData();
    bool factoryReset();
private:
    services::MaintenanceService& service_;
};
}
#endif

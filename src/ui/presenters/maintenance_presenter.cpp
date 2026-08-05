#include "maintenance_presenter.h"
namespace smart_attendance::ui {
bool MaintenancePresenter::clearAttendance() { return static_cast<bool>(service_.clearAttendance()); }
bool MaintenancePresenter::clearEmployees() { return static_cast<bool>(service_.clearEmployees()); }
bool MaintenancePresenter::clearAllData() { return static_cast<bool>(service_.clearAllData()); }
bool MaintenancePresenter::factoryReset() { return static_cast<bool>(service_.factoryReset()); }
}

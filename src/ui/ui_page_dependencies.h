#ifndef SMART_ATTENDANCE_UI_UI_PAGE_DEPENDENCIES_H
#define SMART_ATTENDANCE_UI_UI_PAGE_DEPENDENCIES_H

#include "hal/rtc.h"
#include "ui/presenters/attendance_query_presenter.h"
#include "ui/presenters/department_presenter.h"
#include "ui/presenters/employee_lookup_presenter.h"
#include "ui/presenters/maintenance_presenter.h"
#include "ui/presenters/settings_presenter.h"
#include "ui/presenters/shift_presenter.h"
#include "ui/presenters/system_info_presenter.h"

class UiManager;

namespace smart_attendance::ui {

using DisplayFrameReader = bool (*)(void*, int, int);
using EmployeeFaceRegistrar = bool (*)(const char*, int);
using EmployeeFaceUpdater = bool (*)(int);

struct HomePageDependencies {
    ::UiManager& uiManager;
    hal::IRtc& rtc;
    DisplayFrameReader readDisplayFrame;
};

struct AttendanceDesignPageDependencies {
    ::UiManager& uiManager;
    SettingsPresenter& settings;
    DepartmentPresenter& departments;
    ShiftPresenter& shifts;
};

struct AttendanceStatisticsPageDependencies {
    ::UiManager& uiManager;
    hal::IRtc& rtc;
    EmployeeLookupPresenter& employees;
};

struct RecordQueryPageDependencies {
    ::UiManager& uiManager;
    hal::IRtc& rtc;
    EmployeeLookupPresenter& employees;
    AttendanceQueryPresenter& attendanceQuery;
};

struct SystemInfoPageDependencies {
    ::UiManager& uiManager;
    SystemInfoPresenter& systemInfo;
};

struct SystemSettingsPageDependencies {
    ::UiManager& uiManager;
    hal::IRtc& rtc;
    MaintenancePresenter& maintenance;
};

struct UserManagementPageDependencies {
    ::UiManager& uiManager;
    EmployeeLookupPresenter& employees;
    DepartmentPresenter& departments;
    EmployeeFaceRegistrar registerEmployeeFace;
    EmployeeFaceUpdater updateEmployeeFace;
};

struct MenuPageDependencies {
    ::UiManager& uiManager;
};

/** @brief Application 持有的页面专用依赖集合，不拥有任何被引用对象。 */
struct UiPageDependencies {
    HomePageDependencies home;
    AttendanceDesignPageDependencies attendanceDesign;
    AttendanceStatisticsPageDependencies attendanceStatistics;
    RecordQueryPageDependencies recordQuery;
    SystemInfoPageDependencies systemInfo;
    SystemSettingsPageDependencies systemSettings;
    UserManagementPageDependencies userManagement;
    MenuPageDependencies menu;
};

} // namespace smart_attendance::ui

#endif // SMART_ATTENDANCE_UI_UI_PAGE_DEPENDENCIES_H

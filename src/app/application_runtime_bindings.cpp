#include "application.h"

#include "business/face_demo.h"
#include "services/report_service.h"
#include "ui/ui_app.h"
#include "ui/ui_runtime.h"

namespace smart_attendance::app {

bool Application::configureRuntimeBindings() noexcept {
    if (!services_.initializeReportService()) {
        return false;
    }
    try {
        uiPageDependencies_.home.readDisplayFrame = business_get_display_frame;
        uiPageDependencies_.userManagement.registerEmployeeFace =
            business_register_user;
        uiPageDependencies_.userManagement.updateEmployeeFace =
            business_update_user_face;
        services_.reportService().configureImporter(
            [this](int& invalidTimeCount) {
                return uiImportEmployeeSettings(
                    services_.employeeSettingsImportRepository(),
                    &invalidTimeCount);
            });
        taskManager_.configureReportExporters(
            [this](int userId, const std::string& startDate,
                   const std::string& endDate) {
                return services_.reportService().exportUserReport(
                    userId, startDate, endDate);
            },
            [this](const std::string& startDate,
                   const std::string& endDate) {
                return services_.reportService().exportCustomReport(
                    startDate, endDate);
            },
            [this]() {
                return services_.reportService().exportEmployeeSettings();
            },
            [this](int& invalidTimeCount) {
                return services_.reportService().importEmployeeSettings(
                    invalidTimeCount);
            });
    } catch (...) {
        return false;
    }

    business_configure_punch_service(services_.punchService());
    business_configure_face_recognition_engine(
        services_.faceRecognitionEngine());
    business_configure_face_data_repository(services_.faceDataRepository());
    business_configure_platform_devices(services_.camera(), services_.rtc());
    ui_configure_platform(services_.display(), services_.keypad(), uiManager_);
    uiConfigureManager(uiManager_);
    uiConfigureDeviceServices(services_.rtc(), services_.storage());
    ui_configure_background_jobs(taskManager_.uiBackgroundJobs());
    ui_configure_system_status(taskManager_.uiSystemStatus());
    ui_configure_page_dependencies(uiPageDependencies_);
    return true;
}

} // namespace smart_attendance::app

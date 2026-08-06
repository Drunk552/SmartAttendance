#include "application_services.h"

#include "services/report_service.h"

namespace smart_attendance::app {

bool ApplicationServices::initializeReportService() noexcept {
    if (reportService_) {
        return true;
    }
    try {
        reportService_ = std::make_unique<services::ReportService>(
            storage(), reportDataSource_,
            services::ReportService::EmployeeSettingsImporter{});
        return true;
    } catch (...) {
        return false;
    }
}

services::ReportService& ApplicationServices::reportService() noexcept {
    return *reportService_;
}

} // namespace smart_attendance::app

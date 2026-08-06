#include "report_service.h"

#include "business/report_generator.h"
#include "hal/storage_device.h"

#include <system_error>
#include <utility>

namespace smart_attendance::services {

ReportService::ReportService(hal::IStorageDevice& storage,
                             IReportDataSource& dataSource,
                             EmployeeSettingsImporter importer) noexcept
    : storage_(storage), dataSource_(dataSource), importer_(std::move(importer)) {}

void ReportService::configureImporter(
    EmployeeSettingsImporter importer) noexcept {
    importer_ = std::move(importer);
}

std::filesystem::path ReportService::ensureDirectory(const char* name) {
    const auto result = storage_.ensureDirectory(name);
    if (!result) {
        return {};
    }
    return result.value();
}

bool ReportService::exportUserReport(int userId,
                                     const std::string& startDate,
                                     const std::string& endDate) {
    const auto directory = ensureDirectory("usb_sim");
    if (directory.empty()) {
        return false;
    }
    ReportGenerator generator(dataSource_);
    return generator.exportIndividualAttendanceReport(
        userId, startDate, endDate,
        (directory / ("User_" + std::to_string(userId) + "_Report.xlsx")).string());
}

bool ReportService::exportCustomReport(const std::string& startDate,
                                       const std::string& endDate) {
    const auto directory = ensureDirectory("usb_sim");
    if (directory.empty()) {
        return false;
    }
    ReportGenerator generator(dataSource_);
    return generator.exportAllAttendanceReport(
        startDate, endDate,
        (directory / ("Attendance_Report_All_" + startDate + "_to_" + endDate + ".xlsx")).string());
}

bool ReportService::exportEmployeeSettings() {
    const auto directory = ensureDirectory("usb_settings");
    if (directory.empty()) {
        return false;
    }
    ReportGenerator generator(dataSource_);
    return generator.exportSettingsReport((directory / "员工设置表.xlsx").string());
}

bool ReportService::importEmployeeSettings(int& invalidTimeCount) {
    if (!importer_) {
        return false;
    }
    return importer_(invalidTimeCount);
}

} // namespace smart_attendance::services

#ifndef SMART_ATTENDANCE_SERVICES_REPORT_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_REPORT_SERVICE_H

#include <filesystem>
#include <functional>
#include <string>

namespace smart_attendance::hal { class IStorageDevice; }
namespace smart_attendance::services { class IReportDataSource; }

namespace smart_attendance::services {

/** @brief 软件侧报表和员工设置文件用例；不负责 USB 枚举或挂载。 */
class ReportService final {
public:
    using EmployeeSettingsImporter = std::function<bool(int&)>;

    ReportService(hal::IStorageDevice& storage,
                  IReportDataSource& dataSource,
                  EmployeeSettingsImporter importer) noexcept;

    bool exportUserReport(int userId,
                          const std::string& startDate,
                          const std::string& endDate);
    bool exportCustomReport(const std::string& startDate,
                            const std::string& endDate);
    bool exportEmployeeSettings();
    bool importEmployeeSettings(int& invalidTimeCount);

private:
    std::filesystem::path ensureDirectory(const char* name);

    hal::IStorageDevice& storage_;
    IReportDataSource& dataSource_;
    EmployeeSettingsImporter importer_;
};

} // namespace smart_attendance::services

#endif

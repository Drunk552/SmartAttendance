/**
 * @file application_services.h
 * @brief 声明由 Application 显式持有的服务与基础设施生命周期。
 */

#ifndef SMART_ATTENDANCE_APP_APPLICATION_SERVICES_H
#define SMART_ATTENDANCE_APP_APPLICATION_SERVICES_H

#include "biometric/face/face_recognition_engine.h"
#include "app/platform_factory.h"
#include "services/employee_service.h"
#include "services/config_service.h"
#include "services/department_service.h"
#include "services/punch_service.h"
#include "services/shift_service.h"
#include "services/attendance_query_service.h"
#include "services/maintenance_service.h"
#include "services/system_info_service.h"
#include "storage/sqlite/legacy_employee_repository.h"
#include "storage/sqlite/legacy_config_repository.h"
#include "storage/sqlite/legacy_department_repository.h"
#include "storage/sqlite/legacy_punch_repositories.h"
#include "storage/sqlite/legacy_shift_repository.h"
#include "storage/sqlite/legacy_maintenance_repository.h"
#include "storage/sqlite/legacy_report_data_source.h"
#include "storage/sqlite/legacy_face_data_repository.h"
#include "storage/sqlite/legacy_employee_settings_import_repository.h"
#include "storage/sqlite/legacy_system_info_repository.h"

#include <memory>

namespace smart_attendance::services { class ReportService; }

namespace smart_attendance::app {

/** @brief 旧数据层接入组合根的过渡生命周期适配器。 */
struct DatabaseLifecycle {
    bool (*initialize)();
    void (*close)();
};

/** @brief 旧业务模块接入组合根的过渡生命周期适配器。 */
struct BusinessLifecycle {
    bool (*initialize)();
    void (*shutdown)();
};

/**
 * @brief 显式持有服务和基础设施资源，不创建线程或提供通用查询接口。
 *
 * 当前接管旧数据库与业务模块的生命周期状态，并显式持有统一打卡服务、员工
 * 查询服务及其过渡 Repository。后续可逐个替换适配器，无需改变 TaskManager
 * 的线程职责。
 */
class ApplicationServices final {
public:
    ApplicationServices(DatabaseLifecycle databaseLifecycle,
                        BusinessLifecycle businessLifecycle,
                        PlatformDevices platformDevices) noexcept;
    ~ApplicationServices() noexcept;

    ApplicationServices(const ApplicationServices&) = delete;
    ApplicationServices& operator=(const ApplicationServices&) = delete;
    ApplicationServices(ApplicationServices&&) = delete;
    ApplicationServices& operator=(ApplicationServices&&) = delete;

    bool hasValidDatabaseLifecycle() const noexcept;
    bool hasValidBusinessLifecycle() const noexcept;

    /** @brief 初始化数据库；失败时清理可能已创建的部分资源。 */
    bool initializeDatabase() noexcept;

    /** @brief 初始化业务资源；失败后仍保持活动标记以便显式清理。 */
    bool initializeBusiness() noexcept;

    /** @brief 关闭业务资源；回调异常返回 false 且不会重复调用。 */
    bool shutdownBusiness() noexcept;

    /** @brief 关闭数据库；回调异常返回 false，并由析构路径再做兜底。 */
    bool shutdownDatabase() noexcept;

    /**
     * @brief 返回由本对象持有的统一打卡服务。
     * @note 不转移所有权；调用前数据库必须已初始化，且不得跨越本对象生命周期保存引用。
     */
    services::PunchService& punchService() noexcept;

    /**
     * @brief 返回由本对象持有的员工查询服务。
     * @note 不转移所有权；调用前数据库必须已初始化，且不得跨越本对象生命周期保存引用。
     */
    services::EmployeeService& employeeService() noexcept;

    /** @brief 返回由组合根持有的配置 Repository，供后续配置 Service 迁移使用。 */
    storage::IConfigRepository& configRepository() noexcept;

    /** @brief 返回由组合根持有的配置用例服务。 */
    services::ConfigService& configService() noexcept;

    /** @brief 返回由组合根持有的部门维护和排班读取服务。 */
    services::DepartmentService& departmentService() noexcept;

    services::ShiftService& shiftService() noexcept;
    services::AttendanceQueryService& attendanceQueryService() noexcept;
    services::MaintenanceService& maintenanceService() noexcept;
    bool initializeReportService() noexcept;
    services::ReportService& reportService() noexcept;
    services::SystemInfoService& systemInfoService() noexcept;
    storage::IFaceDataRepository& faceDataRepository() noexcept;
    storage::IEmployeeSettingsImportRepository&
    employeeSettingsImportRepository() noexcept;

    /** @brief 返回由组合根持有的 OpenCV 人脸算法引擎，不转移所有权。 */
    biometric::face::IFaceRecognitionEngine& faceRecognitionEngine() noexcept;

    hal::ICamera& camera() noexcept;
    hal::IDisplay& display() noexcept;
    hal::IKeypad& keypad() noexcept;
    hal::IRtc& rtc() noexcept;
    hal::IStorageDevice& storage() noexcept;
    const hal::DeviceCapabilities& deviceCapabilities() const noexcept;
    bool hasCompletePlatformDevices() const noexcept;

private:
    void cleanupFailedDatabaseInitialization() noexcept;
    void shutdownDatabaseNoexcept() noexcept;

    DatabaseLifecycle databaseLifecycle_;
    BusinessLifecycle businessLifecycle_;
    PlatformDevices platformDevices_;
    biometric::face::FaceRecognitionEngine faceRecognitionEngine_;
    storage::sqlite::LegacyEmployeeRepository employeeRepository_;
    services::EmployeeService employeeService_;
    storage::sqlite::LegacyConfigRepository configRepository_;
    services::ConfigService configService_;
    storage::sqlite::LegacyDepartmentRepository departmentRepository_;
    services::DepartmentService departmentService_;
    storage::sqlite::LegacyShiftRepository shiftManagementRepository_;
    services::ShiftService shiftService_;
    storage::sqlite::LegacyAttendanceQueryRepository attendanceQueryRepository_;
    services::AttendanceQueryService attendanceQueryService_;
    storage::sqlite::LegacyMaintenanceRepository maintenanceRepository_;
    services::MaintenanceService maintenanceService_;
    storage::sqlite::LegacyReportDataSource reportDataSource_;
    storage::sqlite::LegacyEmployeeSettingsImportRepository
        employeeSettingsImportRepository_;
    std::unique_ptr<services::ReportService> reportService_;
    storage::sqlite::LegacySystemInfoRepository systemInfoRepository_;
    services::SystemInfoService systemInfoService_;
    storage::sqlite::LegacyFaceDataRepository faceDataRepository_;
    storage::sqlite::LegacyScheduleRepository scheduleRepository_;
    storage::sqlite::LegacyAttendanceRuleRepository attendanceRuleRepository_;
    storage::sqlite::LegacyAttendanceRepository attendanceRepository_;
    services::PunchService punchService_;
    bool databaseActive_{false};
    bool businessActive_{false};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_APPLICATION_SERVICES_H

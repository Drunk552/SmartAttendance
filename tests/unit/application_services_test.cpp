#include "app/application_services.h"
#include "../mocks/mock_platform_devices.h"

#include "services/punch_service.h"

#include <cstdlib>
#include <iostream>

namespace {

int databaseInitializeCount = 0;
int databaseCloseCount = 0;
int businessInitializeCount = 0;
int businessShutdownCount = 0;
int callOrder = 0;
int businessShutdownOrder = 0;
int databaseCloseOrder = 0;
bool databaseInitializeResult = true;
bool businessInitializeResult = true;
bool databaseCloseShouldThrow = false;
bool businessShutdownShouldThrow = false;

bool initializeDatabase() {
    ++databaseInitializeCount;
    return databaseInitializeResult;
}

void closeDatabase() {
    ++databaseCloseCount;
    databaseCloseOrder = ++callOrder;
    if (databaseCloseShouldThrow) {
        throw 1;
    }
}

bool initializeBusiness() {
    ++businessInitializeCount;
    return businessInitializeResult;
}

void shutdownBusiness() {
    ++businessShutdownCount;
    businessShutdownOrder = ++callOrder;
    if (businessShutdownShouldThrow) {
        throw 1;
    }
}

void resetFakes() {
    databaseInitializeCount = 0;
    databaseCloseCount = 0;
    businessInitializeCount = 0;
    businessShutdownCount = 0;
    callOrder = 0;
    businessShutdownOrder = 0;
    databaseCloseOrder = 0;
    databaseInitializeResult = true;
    businessInitializeResult = true;
    databaseCloseShouldThrow = false;
    businessShutdownShouldThrow = false;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    using smart_attendance::app::ApplicationServices;

    resetFakes();
    {
        ApplicationServices invalidServices(
            {nullptr, closeDatabase},
            {initializeBusiness, nullptr},
            smart_attendance::test::makeMockPlatformDevices());
        require(!invalidServices.hasValidDatabaseLifecycle() &&
                    !invalidServices.hasValidBusinessLifecycle(),
                "incomplete lifecycle adapters must be rejected");
    }

    resetFakes();
    {
        ApplicationServices services(
            {initializeDatabase, closeDatabase},
            {initializeBusiness, shutdownBusiness},
            smart_attendance::test::makeMockPlatformDevices());
        auto& first = services.punchService();
        auto& second = services.punchService();
        require(&first == &second,
                "ApplicationServices must own one stable PunchService instance");
        auto& firstEmployeeService = services.employeeService();
        auto& secondEmployeeService = services.employeeService();
        require(&firstEmployeeService == &secondEmployeeService,
                "ApplicationServices must own one stable EmployeeService instance");
        auto& firstConfigRepository = services.configRepository();
        auto& secondConfigRepository = services.configRepository();
        require(&firstConfigRepository == &secondConfigRepository,
                "ApplicationServices must own one stable config Repository instance");
        auto& firstConfigService = services.configService();
        auto& secondConfigService = services.configService();
        require(&firstConfigService == &secondConfigService,
                "ApplicationServices must own one stable ConfigService instance");
        const auto configUnavailable = firstConfigService.loadCompanyName();
        require(!configUnavailable &&
                    configUnavailable.error() ==
                        smart_attendance::services::ConfigError::ReadFailed,
                "ConfigService must observe a closed database without opening it");
        auto& firstDepartmentService = services.departmentService();
        auto& secondDepartmentService = services.departmentService();
        require(&firstDepartmentService == &secondDepartmentService,
                "ApplicationServices must own one stable DepartmentService instance");
        const auto departmentsUnavailable =
            firstDepartmentService.listDepartments();
        require(!departmentsUnavailable &&
                    departmentsUnavailable.error() ==
                        smart_attendance::services::DepartmentError::ReadFailed,
                "DepartmentService must observe a closed database without opening it");
        require(&services.shiftService() == &services.shiftService(),
                "ApplicationServices must own one stable ShiftService instance");
        require(!services.shiftService().listShifts(),
                "ShiftService must observe a closed database without opening it");
        require(&services.attendanceQueryService() ==
                    &services.attendanceQueryService(),
                "ApplicationServices must own one stable attendance query service");
        require(!services.attendanceQueryService().query(-1, 0, 1),
                "attendance queries must fail while the database is closed");
        require(&services.maintenanceService() == &services.maintenanceService(),
                "ApplicationServices must own one stable maintenance service");
        require(!services.maintenanceService().clearAttendance(),
                "maintenance writes must fail while the database is closed");
        auto& firstFaceEngine = services.faceRecognitionEngine();
        auto& secondFaceEngine = services.faceRecognitionEngine();
        require(&firstFaceEngine == &secondFaceEngine &&
                    !firstFaceEngine.isTrained(),
                "ApplicationServices must own one stable face engine instance");
        require(services.hasCompletePlatformDevices() &&
                    &services.camera() == &services.camera() &&
                    &services.rtc() == &services.rtc(),
                "ApplicationServices must own one complete stable platform set");
        const auto employeeUnavailable = firstEmployeeService.findById(1);
        require(!employeeUnavailable &&
                    employeeUnavailable.error() ==
                        smart_attendance::services::EmployeeError::ReadFailed,
                "EmployeeService must observe a closed database without opening it");
        const auto unavailable = first.punch({1, 1000, 8 * 60});
        require(!unavailable &&
                    unavailable.error() ==
                        smart_attendance::services::PunchError::ScheduleReadFailed,
                "PunchService must observe a closed database without opening it");
        require(databaseInitializeCount == 0,
                "accessing owned services must not initialize the database implicitly");
    }

    resetFakes();
    databaseInitializeResult = false;
    {
        ApplicationServices services(
            {initializeDatabase, closeDatabase},
            {initializeBusiness, shutdownBusiness},
            smart_attendance::test::makeMockPlatformDevices());
        require(!services.initializeDatabase(),
                "database initialization failure must propagate");
        require(databaseInitializeCount == 1 && databaseCloseCount == 1,
                "partial database initialization must close exactly once");
    }
    require(databaseCloseCount == 1,
            "destructor must not repeat completed database cleanup");

    resetFakes();
    businessInitializeResult = false;
    {
        ApplicationServices services(
            {initializeDatabase, closeDatabase},
            {initializeBusiness, shutdownBusiness},
            smart_attendance::test::makeMockPlatformDevices());
        require(services.initializeDatabase(),
                "database must initialize before business resources");
        require(!services.initializeBusiness(),
                "business initialization failure must propagate");
        require(services.shutdownBusiness() && services.shutdownDatabase(),
                "partial business state and database must remain cleanable");
        require(businessShutdownOrder < databaseCloseOrder,
                "business resources must close before the database");
    }

    resetFakes();
    businessShutdownShouldThrow = true;
    {
        ApplicationServices services(
            {initializeDatabase, closeDatabase},
            {initializeBusiness, shutdownBusiness},
            smart_attendance::test::makeMockPlatformDevices());
        require(services.initializeDatabase() && services.initializeBusiness(),
                "shutdown failure test must initialize services");
        require(!services.shutdownBusiness(),
                "business shutdown exception must be reported");
        require(services.shutdownDatabase(),
                "database must still close after business shutdown failure");
    }
    require(businessShutdownCount == 1 && databaseCloseCount == 1,
            "destructor must not retry completed shutdown callbacks");

    resetFakes();
    {
        ApplicationServices services(
            {initializeDatabase, closeDatabase},
            {initializeBusiness, shutdownBusiness},
            smart_attendance::test::makeMockPlatformDevices());
        require(services.initializeDatabase(),
                "database close retry test must initialize the database");
        databaseCloseShouldThrow = true;
        require(!services.shutdownDatabase(),
                "database close exception must be reported");
        databaseCloseShouldThrow = false;
    }
    require(databaseCloseCount == 2,
            "destructor must retry one database close left incomplete");

    std::cout << "[PASSED] application services lifecycle ownership\n";
    return EXIT_SUCCESS;
}

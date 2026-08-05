/**
 * @file application_lifecycle_test.cpp
 * @brief 验证 Application 的状态、运行目录和资源释放顺序。
 */

#include "app/application.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using smart_attendance::app::Application;
using smart_attendance::app::ApplicationInitError;
using smart_attendance::app::ApplicationLoop;
using smart_attendance::app::ApplicationRunError;
using smart_attendance::app::ApplicationState;
using smart_attendance::app::DatabaseLifecycle;
using smart_attendance::app::BusinessLifecycle;
using smart_attendance::app::MonitorWorkerLifecycle;
using smart_attendance::app::UiSystemStatusMailbox;
using smart_attendance::app::UiLifecycle;
using smart_attendance::app::WorkerLifecycle;

namespace {

std::atomic<int> databaseInitializeCount{0};
std::atomic<int> databaseCloseCount{0};
std::atomic<int> uiInitializeCount{0};
std::atomic<int> uiShutdownCount{0};
std::atomic<int> taskInitializeCount{0};
std::atomic<int> businessShutdownCount{0};
std::atomic<int> monitorRunCount{0};
std::atomic<int> monitorExitCount{0};
std::atomic<int> monitorWakeCount{0};
std::atomic<int> frameDeliveryRunCount{0};
std::atomic<int> frameDeliveryExitCount{0};
std::atomic<int> captureRunCount{0};
std::atomic<int> captureExitCount{0};
std::atomic<int> writerRunCount{0};
std::atomic<int> writerExitCount{0};
std::atomic<int> writerWakeCount{0};
std::atomic<int> userReportExportCount{0};
std::atomic<int> lifecycleOrder{0};
std::atomic<int> userReportExportOrder{0};
std::atomic<int> monitorExitOrder{0};
std::atomic<int> frameDeliveryExitOrder{0};
std::atomic<int> captureExitOrder{0};
std::atomic<int> writerWakeOrder{0};
std::atomic<int> writerExitOrder{0};
std::atomic<int> databaseCloseOrder{0};
std::atomic<int> databaseInitializeOrder{0};
std::atomic<int> uiInitializeOrder{0};
std::atomic<int> uiShutdownOrder{0};
std::atomic<int> taskInitializeOrder{0};
std::atomic<int> businessShutdownOrder{0};
std::atomic<int> loopIterationCount{0};
std::atomic<int> firstLoopIterationOrder{0};
bool databaseInitializeResult = true;
bool uiInitializeShouldThrow = false;
bool uiShutdownShouldThrow = false;
bool businessInitializeResult = true;
bool businessShutdownShouldThrow = false;
bool loopIterationShouldThrow = false;
bool captureShouldThrow = false;

bool fakeDatabaseInitialize() {
    ++databaseInitializeCount;
    databaseInitializeOrder = ++lifecycleOrder;
    return databaseInitializeResult;
}

void fakeDatabaseClose() {
    ++databaseCloseCount;
    databaseCloseOrder = ++lifecycleOrder;
}

bool fakeTaskInitialize() {
    ++taskInitializeCount;
    taskInitializeOrder = ++lifecycleOrder;
    return businessInitializeResult;
}

void fakeBusinessShutdown() {
    ++businessShutdownCount;
    businessShutdownOrder = ++lifecycleOrder;
    if (businessShutdownShouldThrow) {
        throw 1;
    }
}

void fakeUiInitialize() {
    ++uiInitializeCount;
    uiInitializeOrder = ++lifecycleOrder;
    if (uiInitializeShouldThrow) {
        throw 1;
    }
}

void fakeUiShutdown() {
    ++uiShutdownCount;
    uiShutdownOrder = ++lifecycleOrder;
    if (uiShutdownShouldThrow) {
        throw 1;
    }
}

bool fakeShouldStop() {
    return loopIterationCount.load() >= 3;
}

void fakeRunOnce() {
    if (++loopIterationCount == 1) {
        firstLoopIterationOrder = ++lifecycleOrder;
    }
    if (loopIterationShouldThrow) {
        throw 1;
    }
}

bool fakeUserReportExporter(int,
                            const std::string&,
                            const std::string&) {
    ++userReportExportCount;
    userReportExportOrder = ++lifecycleOrder;
    return true;
}

bool fakeCustomReportExporter(const std::string&,
                              const std::string&) {
    return true;
}

bool fakeEmployeeSettingsExporter() {
    return true;
}

bool fakeEmployeeSettingsImporter(int&) {
    return true;
}

void fakeMonitorRun(const std::atomic<bool>& stopRequested,
                    UiSystemStatusMailbox& mailbox) {
    ++monitorRunCount;
    mailbox.publishTime("08:00:00", "Mon");
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++monitorExitCount;
    monitorExitOrder = ++lifecycleOrder;
}

void fakeMonitorWake() {
    ++monitorWakeCount;
}

void fakeFrameDeliveryRun(const std::atomic<bool>& stopRequested) {
    ++frameDeliveryRunCount;
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++frameDeliveryExitCount;
    frameDeliveryExitOrder = ++lifecycleOrder;
}

void fakeCaptureRun(const std::atomic<bool>& stopRequested) {
    ++captureRunCount;
    if (captureShouldThrow) {
        throw 1;
    }
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++captureExitCount;
    captureExitOrder = ++lifecycleOrder;
}

void fakeWriterRun(const std::atomic<bool>& stopRequested) {
    ++writerRunCount;
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++writerExitCount;
    writerExitOrder = ++lifecycleOrder;
}

void fakeWriterWake() {
    ++writerWakeCount;
    writerWakeOrder = ++lifecycleOrder;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void resetFakes(bool databaseResult) {
    databaseInitializeCount = 0;
    databaseCloseCount = 0;
    uiInitializeCount = 0;
    uiShutdownCount = 0;
    taskInitializeCount = 0;
    businessShutdownCount = 0;
    monitorRunCount = 0;
    monitorExitCount = 0;
    monitorWakeCount = 0;
    frameDeliveryRunCount = 0;
    frameDeliveryExitCount = 0;
    captureRunCount = 0;
    captureExitCount = 0;
    writerRunCount = 0;
    writerExitCount = 0;
    writerWakeCount = 0;
    userReportExportCount = 0;
    lifecycleOrder = 0;
    userReportExportOrder = 0;
    monitorExitOrder = 0;
    frameDeliveryExitOrder = 0;
    captureExitOrder = 0;
    writerWakeOrder = 0;
    writerExitOrder = 0;
    databaseCloseOrder = 0;
    databaseInitializeOrder = 0;
    uiInitializeOrder = 0;
    uiShutdownOrder = 0;
    taskInitializeOrder = 0;
    businessShutdownOrder = 0;
    loopIterationCount = 0;
    firstLoopIterationOrder = 0;
    databaseInitializeResult = databaseResult;
    uiInitializeShouldThrow = false;
    uiShutdownShouldThrow = false;
    businessInitializeResult = true;
    businessShutdownShouldThrow = false;
    loopIterationShouldThrow = false;
    captureShouldThrow = false;
}

std::filesystem::path uniqueRuntimeDirectory() {
    const auto suffix = std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count();
    return std::filesystem::temp_directory_path() /
           ("smartattendance_application_test_" + std::to_string(suffix));
}

} // namespace

int main() {
    const auto originalDirectory = std::filesystem::current_path();
    const auto runtimeDirectory = uniqueRuntimeDirectory();
    const DatabaseLifecycle databaseLifecycle{
        fakeDatabaseInitialize,
        fakeDatabaseClose
    };
    const UiLifecycle uiLifecycle{fakeUiInitialize, fakeUiShutdown};
    const ApplicationLoop applicationLoop{fakeShouldStop, fakeRunOnce};
    const BusinessLifecycle businessLifecycle{fakeTaskInitialize, fakeBusinessShutdown};
    const MonitorWorkerLifecycle monitorLifecycle{fakeMonitorRun, fakeMonitorWake};
    const WorkerLifecycle frameDeliveryLifecycle{fakeFrameDeliveryRun, nullptr};
    const WorkerLifecycle captureLifecycle{fakeCaptureRun, nullptr};
    const WorkerLifecycle writerLifecycle{fakeWriterRun, fakeWriterWake};

    resetFakes(true);
    {
        Application invalidUiApplication(
            databaseLifecycle,
            {fakeUiInitialize, nullptr},
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(invalidUiApplication.initialize() ==
                    ApplicationInitError::InvalidUiLifecycle,
                "incomplete UI lifecycle must be rejected");
        require(databaseInitializeCount == 0 && uiInitializeCount == 0,
                "invalid UI configuration must not initialize resources");
    }

    resetFakes(true);
    {
        Application invalidLoopApplication(
            databaseLifecycle,
            uiLifecycle,
            {nullptr, nullptr},
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(invalidLoopApplication.initialize() ==
                    ApplicationInitError::InvalidApplicationLoop,
                "missing application loop callbacks must be rejected");
        require(databaseInitializeCount == 0 && uiInitializeCount == 0,
                "invalid application loop must not initialize resources");
    }

    resetFakes(true);
    {
        Application invalidBusinessApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            {fakeTaskInitialize, nullptr},
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(invalidBusinessApplication.initialize() ==
                    ApplicationInitError::InvalidBusinessLifecycle,
                "incomplete business lifecycle must be rejected");
        require(databaseInitializeCount == 0 && uiInitializeCount == 0 &&
                    taskInitializeCount == 0,
                "invalid business lifecycle must not initialize resources");
    }

    resetFakes(false);
    {
        Application failedApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedApplication.initialize() ==
                    ApplicationInitError::DatabaseInitializationFailed,
                "database initialization failure must be reported");
        require(failedApplication.state() == ApplicationState::Created,
                "failed initialization must keep Created state");
        require(databaseInitializeCount == 1 && databaseCloseCount == 1,
                "failed database initialization must clean partial resources");
        require(taskInitializeCount == 0 && monitorRunCount == 0 &&
                    uiInitializeCount == 0 &&
                    uiShutdownCount == 0 &&
                    frameDeliveryRunCount == 0 && captureRunCount == 0 &&
                    writerRunCount == 0,
                "database failure must not touch tasks");
    }
    require(databaseCloseCount == 1,
            "destructor must not close an already cleaned database twice");

    resetFakes(true);
    uiInitializeShouldThrow = true;
    {
        Application failedUiApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedUiApplication.initialize() ==
                    ApplicationInitError::UiInitializationFailed,
                "UI initialization exception must be reported");
        require(failedUiApplication.state() == ApplicationState::Created,
                "failed UI initialization must keep Created state");
        require(databaseInitializeCount == 1 && uiInitializeCount == 1 &&
                    databaseCloseCount == 1,
                "UI failure must close the initialized database exactly once");
        require(databaseInitializeOrder < uiInitializeOrder &&
                    uiInitializeOrder < databaseCloseOrder,
                "UI failure cleanup must preserve initialization order");
        require(taskInitializeCount == 0 && monitorRunCount == 0 &&
                    uiShutdownCount == 0 &&
                    frameDeliveryRunCount == 0 && captureRunCount == 0 &&
                    writerRunCount == 0,
                "UI failure must not start business tasks");
    }
    require(databaseCloseCount == 1,
            "UI failure destructor must not close the database twice");

    resetFakes(true);
    businessInitializeResult = false;
    {
        Application failedBusinessApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedBusinessApplication.initialize() ==
                    ApplicationInitError::None,
                "business failure test must initialize database and UI");
        require(!failedBusinessApplication.markRunning(),
                "reported business initialization failure must propagate");
        require(failedBusinessApplication.requestStop() &&
                    failedBusinessApplication.stop(),
                "partial business initialization must still be cleaned");
        require(taskInitializeCount == 1 && businessShutdownCount == 1 &&
                    uiShutdownCount == 1 && databaseCloseCount == 1,
                "business initialization failure must close every active layer");
        require(monitorRunCount == 0 && frameDeliveryRunCount == 0 &&
                    captureRunCount == 0 && writerRunCount == 0,
                "business initialization failure must not start workers");
        require(uiShutdownOrder < businessShutdownOrder &&
                    businessShutdownOrder < databaseCloseOrder,
                "partial business cleanup must preserve dependency order");
    }

    resetFakes(true);
    {
        Application application(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);

        require(application.state() == ApplicationState::Created,
                "application must start in Created state");
        require(!application.markRunning(),
                "application must not run before initialization");
        require(application.run() == ApplicationRunError::InvalidState,
                "main loop must not run before task startup");
        require(application.initialize() == ApplicationInitError::None,
                "application initialization must succeed");
        require(std::filesystem::current_path() == runtimeDirectory,
                "initialization must switch to runtime directory");
        require(databaseInitializeCount == 1,
                "database must initialize exactly once");
        require(uiInitializeCount == 1 &&
                    databaseInitializeOrder < uiInitializeOrder,
                "UI must initialize once after the database");
        require(application.initialize() == ApplicationInitError::InvalidState,
                "repeated initialization must be rejected");

        require(application.markRunning(),
                "Initialized -> Running transition must succeed");
        require(taskInitializeCount == 1,
                "task resources must initialize exactly once");
        require(uiInitializeOrder < taskInitializeOrder,
                "UI must initialize before business tasks start");
        require(application.run() == ApplicationRunError::None,
                "application loop must finish when stop condition is observed");
        require(loopIterationCount == 3 &&
                    taskInitializeOrder < firstLoopIterationOrder,
                "main loop must run after business tasks start");
        std::uint64_t reportRequestId = 0;
        require(application.uiBackgroundJobs().submitUserReport(
                    smart_attendance::app::UiBackgroundJobOwner::RecordQuery,
                    1, "2026-01-01", "2026-01-02", reportRequestId) ==
                    smart_attendance::app::UiBackgroundJobSubmitError::None,
                "application-owned report queue must accept work while running");
        require(application.requestStop(),
                "Running -> StopRequested transition must succeed");
        require(monitorWakeCount == 1,
                "monitor must be woken by the stop request");
        require(writerWakeCount == 0,
                "writer must remain active until capture joins");
        require(!application.markRunning(),
                "application must not restart after stop request");
        require(application.stop(),
                "stop must join workers and close the database");
        require(captureRunCount == 1 && captureExitCount == 1,
                "capture worker must start and exit exactly once");
        require(monitorRunCount == 1 && monitorExitCount == 1,
                "monitor worker must start and exit exactly once");
        smart_attendance::app::UiSystemStatusSnapshot statusSnapshot{};
        require(application.uiSystemStatus().tryConsume(statusSnapshot) &&
                    statusSnapshot.timeText == "08:00:00" &&
                    statusSnapshot.weekdayText == "Mon",
                "application must expose the monitor worker's latest UI status");
        require(frameDeliveryRunCount == 1 && frameDeliveryExitCount == 1,
                "frame delivery worker must start and exit exactly once");
        require(writerRunCount == 1 && writerExitCount == 1,
                "writer worker must start and exit exactly once");
        require(writerWakeCount == 1,
                "writer must be woken exactly once");
        smart_attendance::app::UiBackgroundJobResult reportResult{};
        require(application.uiBackgroundJobs().tryPopResult(reportResult) &&
                    reportResult.requestId == reportRequestId &&
                    reportResult.owner ==
                        smart_attendance::app::UiBackgroundJobOwner::RecordQuery &&
                    reportResult.type ==
                        smart_attendance::app::UiBackgroundJobType::User &&
                    reportResult.success,
                "accepted report work must finish before task shutdown completes");
        require(userReportExportCount == 1 &&
                    userReportExportOrder < uiShutdownOrder,
                "report export must finish before the UI shuts down");
        require(monitorExitOrder < writerWakeOrder &&
                    frameDeliveryExitOrder < writerWakeOrder &&
                    captureExitOrder < writerWakeOrder &&
                    writerWakeOrder < uiShutdownOrder &&
                    writerExitOrder < uiShutdownOrder &&
                    uiShutdownOrder < businessShutdownOrder &&
                    businessShutdownOrder < databaseCloseOrder,
                "workers, UI, business and database must stop in dependency order");
        require(application.state() == ApplicationState::Stopped,
                "application must finish in Stopped state");
        require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                    databaseCloseCount == 1,
                "UI, business and database must close exactly once");
        require(!application.requestStop(),
                "Stopped application must reject another stop request");
    }
    require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                databaseCloseCount == 1,
            "destructor must not close stopped resources twice");

    resetFakes(true);
    {
        Application failedLoopApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedLoopApplication.initialize() ==
                    ApplicationInitError::None &&
                    failedLoopApplication.markRunning(),
                "loop failure test must start the application");
        loopIterationShouldThrow = true;
        require(failedLoopApplication.run() == ApplicationRunError::LoopFailed,
                "main loop callback exception must be contained");
        require(failedLoopApplication.requestStop() &&
                    failedLoopApplication.stop(),
                "loop failure must still allow orderly resource cleanup");
    }
    require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                databaseCloseCount == 1 &&
                uiShutdownOrder < businessShutdownOrder &&
                businessShutdownOrder < databaseCloseOrder &&
                monitorExitCount == 1 &&
                frameDeliveryExitCount == 1 && captureExitCount == 1 &&
                writerExitCount == 1,
            "loop failure cleanup must join workers and close the database");

    resetFakes(true);
    uiShutdownShouldThrow = true;
    {
        Application failedUiShutdownApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedUiShutdownApplication.initialize() ==
                    ApplicationInitError::None &&
                    failedUiShutdownApplication.markRunning(),
                "UI shutdown failure test must start the application");
        require(failedUiShutdownApplication.requestStop(),
                "UI shutdown failure test must request worker stop");
        require(!failedUiShutdownApplication.stop(),
                "UI shutdown exception must be reported");
        require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                    databaseCloseCount == 1 &&
                    uiShutdownOrder < businessShutdownOrder &&
                    businessShutdownOrder < databaseCloseOrder,
                "business and database must still close after UI shutdown failure");
    }
    require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                databaseCloseCount == 1,
            "destructor must not retry partially completed UI shutdown");

    resetFakes(true);
    businessShutdownShouldThrow = true;
    {
        Application failedBusinessShutdownApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedBusinessShutdownApplication.initialize() ==
                    ApplicationInitError::None &&
                    failedBusinessShutdownApplication.markRunning(),
                "business shutdown failure test must start the application");
        require(failedBusinessShutdownApplication.requestStop(),
                "business shutdown failure test must request worker stop");
        require(!failedBusinessShutdownApplication.stop(),
                "business shutdown exception must be reported");
        require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                    databaseCloseCount == 1 &&
                    uiShutdownOrder < businessShutdownOrder &&
                    businessShutdownOrder < databaseCloseOrder,
                "database must still close after business shutdown failure");
    }
    require(businessShutdownCount == 1 && databaseCloseCount == 1,
            "destructor must not retry partially completed business shutdown");

    resetFakes(true);
    captureShouldThrow = true;
    {
        Application failedWorkerApplication(
            databaseLifecycle,
            uiLifecycle,
            applicationLoop,
            businessLifecycle,
            fakeUserReportExporter,
            fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle,
            writerLifecycle,
            runtimeDirectory);
        require(failedWorkerApplication.initialize() ==
                    ApplicationInitError::None,
                "worker failure test must initialize database");
        require(failedWorkerApplication.markRunning(),
                "worker threads must start before failure is observed");
    }
    require(monitorExitCount == 1 && frameDeliveryExitCount == 1 &&
                writerExitCount == 1,
            "destructor must stop monitor, frame delivery and writer after capture failure");
    require(uiShutdownCount == 1 && businessShutdownCount == 1 &&
                databaseCloseCount == 1 &&
                writerExitOrder < uiShutdownOrder &&
                uiShutdownOrder < businessShutdownOrder &&
                businessShutdownOrder < databaseCloseOrder,
            "destructor must close UI, business and database after all workers join");

    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(runtimeDirectory);

    std::cout << "[PASSED] application worker, UI, business and database lifecycle\n";
    return EXIT_SUCCESS;
}

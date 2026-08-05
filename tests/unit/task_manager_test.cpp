/**
 * @file task_manager_test.cpp
 * @brief 验证 TaskManager 对监控、帧投递、采集和数据库写 Worker 的生命周期所有权。
 */

#include "app/task_manager.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

using smart_attendance::app::TaskManager;
using smart_attendance::app::TaskManagerState;
using smart_attendance::app::MonitorWorkerLifecycle;
using smart_attendance::app::UiSystemStatusMailbox;
using smart_attendance::app::WorkerLifecycle;

namespace {

std::atomic<int> monitorRunCount{0};
std::atomic<int> monitorExitCount{0};
std::atomic<int> monitorWakeCount{0};
std::atomic<int> frameDeliveryRunCount{0};
std::atomic<int> frameDeliveryExitCount{0};
std::atomic<int> captureRunCount{0};
std::atomic<int> captureExitCount{0};
std::atomic<int> databaseRunCount{0};
std::atomic<int> databaseExitCount{0};
std::atomic<int> databaseWakeCount{0};
std::atomic<int> callbackOrder{0};
std::atomic<int> frameDeliveryExitOrder{0};
std::atomic<int> captureExitOrder{0};
std::atomic<int> databaseWakeOrder{0};
bool throwInFrameDeliveryWorker = false;
bool throwInCaptureWorker = false;

bool fakeUserReportExporter(int,
                            const std::string&,
                            const std::string&) {
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
}

void fakeMonitorWake() {
    ++monitorWakeCount;
}

void fakeFrameDeliveryRun(const std::atomic<bool>& stopRequested) {
    ++frameDeliveryRunCount;
    if (throwInFrameDeliveryWorker) {
        throw 1;
    }
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++frameDeliveryExitCount;
    frameDeliveryExitOrder = ++callbackOrder;
}

void fakeCaptureRun(const std::atomic<bool>& stopRequested) {
    ++captureRunCount;
    if (throwInCaptureWorker) {
        throw 1;
    }
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++captureExitCount;
    captureExitOrder = ++callbackOrder;
}

void fakeDatabaseRun(const std::atomic<bool>& stopRequested) {
    ++databaseRunCount;
    while (!stopRequested.load()) {
        std::this_thread::yield();
    }
    ++databaseExitCount;
}

void fakeDatabaseWake() {
    ++databaseWakeCount;
    databaseWakeOrder = ++callbackOrder;
}

void resetFake() {
    monitorRunCount = 0;
    monitorExitCount = 0;
    monitorWakeCount = 0;
    frameDeliveryRunCount = 0;
    frameDeliveryExitCount = 0;
    captureRunCount = 0;
    captureExitCount = 0;
    databaseRunCount = 0;
    databaseExitCount = 0;
    databaseWakeCount = 0;
    callbackOrder = 0;
    frameDeliveryExitOrder = 0;
    captureExitOrder = 0;
    databaseWakeOrder = 0;
    throwInFrameDeliveryWorker = false;
    throwInCaptureWorker = false;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    const MonitorWorkerLifecycle monitorLifecycle{fakeMonitorRun, fakeMonitorWake};
    const WorkerLifecycle frameDeliveryLifecycle{fakeFrameDeliveryRun, nullptr};
    const WorkerLifecycle captureLifecycle{fakeCaptureRun, nullptr};
    const WorkerLifecycle databaseLifecycle{
        fakeDatabaseRun,
        fakeDatabaseWake
    };

    resetFake();
    {
        TaskManager manager(
            fakeUserReportExporter, fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle, databaseLifecycle);
        require(manager.state() == TaskManagerState::Created,
                "task manager must start in Created state");
        require(!manager.join(),
                "join before a stop request must be rejected");
        require(manager.start(), "first start must succeed");
        require(!manager.start(), "repeated start must be rejected");

        require(manager.requestStop(), "requestStop must succeed");
        require(manager.state() == TaskManagerState::StopRequested,
                "requestStop must change the state");
        require(monitorWakeCount == 1,
                "monitor worker must be woken by the stop request");
        require(databaseWakeCount == 0,
                "database writer must remain active until capture joins");
        require(manager.join(), "join after requestStop must succeed");
        require(captureRunCount == 1 && captureExitCount == 1,
                "capture worker must start and exit exactly once");
        require(monitorRunCount == 1 && monitorExitCount == 1,
                "monitor worker must start and exit exactly once");
        require(frameDeliveryRunCount == 1 && frameDeliveryExitCount == 1,
                "frame delivery worker must start and exit exactly once");
        require(databaseRunCount == 1 && databaseExitCount == 1,
                "database worker must start and exit exactly once");
        require(databaseWakeCount == 1,
                "database worker must be woken exactly once");
        require(frameDeliveryExitOrder < databaseWakeOrder &&
                    captureExitOrder < databaseWakeOrder,
                "frame delivery and capture must exit before database shutdown begins");
        require(manager.join(), "repeated join must be harmless");
        require(databaseWakeCount == 1,
                "repeated join must not wake the database worker twice");
    }

    resetFake();
    {
        TaskManager manager(
            fakeUserReportExporter, fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle, databaseLifecycle);
        require(manager.start(), "destructor cleanup start must succeed");
    }
    require(monitorExitCount == 1 && frameDeliveryExitCount == 1 &&
                captureExitCount == 1 && databaseExitCount == 1,
            "destructor must join all running workers");

    resetFake();
    throwInFrameDeliveryWorker = true;
    {
        TaskManager manager(
            fakeUserReportExporter, fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle, databaseLifecycle);
        require(manager.start(),
                "thread creation must succeed before frame delivery failure");
        require(manager.requestStop(),
                "frame delivery failure must not block stop request");
        require(!manager.join(),
                "frame delivery exception must be reported by join");
        require(captureExitCount == 1 && databaseExitCount == 1,
                "other workers must still stop after frame delivery failure");
    }

    resetFake();
    throwInCaptureWorker = true;
    {
        TaskManager manager(
            fakeUserReportExporter, fakeCustomReportExporter,
            fakeEmployeeSettingsExporter,
            fakeEmployeeSettingsImporter,
            monitorLifecycle,
            frameDeliveryLifecycle,
            captureLifecycle, databaseLifecycle);
        require(manager.start(),
                "thread creation must succeed before capture failure");
        require(manager.requestStop(),
                "capture failure must not block stop request");
        require(!manager.join(),
                "capture exception must be reported by join");
        require(manager.state() == TaskManagerState::Joined,
                "failed capture worker must still be joined");
        require(databaseExitCount == 1,
                "database worker must still stop after capture failure");
        require(!manager.join(),
                "repeated join must preserve worker failure result");
    }

    resetFake();
    {
        TaskManager manager(
            nullptr, nullptr, nullptr, nullptr,
            {nullptr, nullptr}, {nullptr, nullptr},
            {nullptr, nullptr}, {nullptr, nullptr});
        require(!manager.start(), "invalid callbacks must be rejected");
    }

    std::cout << "[PASSED] task manager owns monitor, frame delivery, capture and database workers\n";
    return EXIT_SUCCESS;
}

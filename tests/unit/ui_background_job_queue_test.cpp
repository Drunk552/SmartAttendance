/**
 * @file ui_background_job_queue_test.cpp
 * @brief 验证 UI 后台任务队列的容量、结果和停止行为。
 */

#include "app/ui_background_job_queue.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using smart_attendance::app::UiBackgroundJobQueue;
using smart_attendance::app::UiBackgroundJobOwner;
using smart_attendance::app::UiBackgroundJobType;
using smart_attendance::app::UiBackgroundJobSubmitError;
using smart_attendance::app::UiBackgroundJobResult;

namespace {

std::atomic<int> exportCount{0};
std::atomic<int> customExportCount{0};
std::atomic<int> settingsExportCount{0};
std::atomic<int> settingsImportCount{0};

bool fakeExporter(int userId,
                  const std::string&,
                  const std::string&) {
    ++exportCount;
    if (userId == 3) {
        throw 1;
    }
    return userId != 2;
}

bool fakeCustomExporter(const std::string&,
                        const std::string&) {
    ++customExportCount;
    return true;
}

bool fakeSettingsExporter() {
    ++settingsExportCount;
    return true;
}

bool fakeSettingsImporter(int& invalidTimeCount) {
    ++settingsImportCount;
    invalidTimeCount = 3;
    return true;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    UiBackgroundJobQueue queue(
        fakeExporter, fakeCustomExporter, fakeSettingsExporter,
        fakeSettingsImporter);
    std::uint64_t requestIds[UiBackgroundJobQueue::kCapacity]{};

    std::uint64_t ignoredId = 0;
    require(queue.submitUserReport(
                UiBackgroundJobOwner::RecordQuery,
                0, "2026-01-01", "2026-01-02", ignoredId) ==
                UiBackgroundJobSubmitError::InvalidArgument,
            "invalid user id must be rejected");

    for (std::size_t index = 0; index < UiBackgroundJobQueue::kCapacity; ++index) {
        require(queue.submitUserReport(
                    UiBackgroundJobOwner::RecordQuery,
                    static_cast<int>(index + 1),
                    "2026-01-01",
                    "2026-01-02",
                    requestIds[index]) == UiBackgroundJobSubmitError::None,
                "requests within capacity must be accepted");
    }
    require(queue.submitUserReport(
                UiBackgroundJobOwner::RecordQuery,
                5, "2026-01-01", "2026-01-02", ignoredId) ==
                UiBackgroundJobSubmitError::QueueFull,
            "request beyond capacity must be rejected");

    std::atomic<bool> stopRequested{false};
    std::thread worker([&]() { queue.run(stopRequested); });

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while (exportCount.load() != static_cast<int>(UiBackgroundJobQueue::kCapacity) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    require(exportCount.load() == static_cast<int>(UiBackgroundJobQueue::kCapacity),
            "worker must execute every accepted request");
    require(queue.submitUserReport(
                UiBackgroundJobOwner::RecordQuery,
                5, "2026-01-01", "2026-01-02", ignoredId) ==
                UiBackgroundJobSubmitError::QueueFull,
            "unconsumed results must count toward the bound");

    for (std::size_t index = 0; index < UiBackgroundJobQueue::kCapacity; ++index) {
        UiBackgroundJobResult result{};
        require(queue.tryPopResult(result),
                "each accepted request must produce a result");
        require(result.requestId == requestIds[index],
                "results must preserve request order");
        require(result.owner == UiBackgroundJobOwner::RecordQuery,
                "user report result must preserve its UI owner");
        require(result.type == UiBackgroundJobType::User,
                "user report result must preserve its task type");
        const bool expectedSuccess = index == 0 || index == 3;
        require(result.success == expectedSuccess,
                "failure and exception results must be reported as false");
        require(result.invalidTimeCount == 0,
                "report result must not contain import diagnostics");
    }


    std::uint64_t customRequestId = 0;
    require(queue.submitCustomReport(
                UiBackgroundJobOwner::AttendanceStatistics,
                "2026-01-01", "2026-01-02", customRequestId) ==
                UiBackgroundJobSubmitError::None,
            "custom report request must share the bounded queue");
    UiBackgroundJobResult customResult{};
    const auto customDeadline = std::chrono::steady_clock::now() +
                                std::chrono::seconds(2);
    while (!queue.tryPopResult(customResult) &&
           std::chrono::steady_clock::now() < customDeadline) {
        std::this_thread::yield();
    }
    require(customExportCount.load() == 1 &&
                customResult.requestId == customRequestId &&
                customResult.owner == UiBackgroundJobOwner::AttendanceStatistics &&
                customResult.type == UiBackgroundJobType::Custom &&
                customResult.success,
            "custom report request must produce a successful result");

    std::uint64_t settingsRequestId = 0;
    require(queue.submitEmployeeSettingsExport(
                UiBackgroundJobOwner::AttendanceStatistics,
                settingsRequestId) == UiBackgroundJobSubmitError::None,
            "employee settings export must share the bounded queue");
    UiBackgroundJobResult settingsResult{};
    const auto settingsDeadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(2);
    while (!queue.tryPopResult(settingsResult) &&
           std::chrono::steady_clock::now() < settingsDeadline) {
        std::this_thread::yield();
    }
    require(settingsExportCount.load() == 1 &&
                settingsResult.requestId == settingsRequestId &&
                settingsResult.owner ==
                    UiBackgroundJobOwner::AttendanceStatistics &&
                settingsResult.type == UiBackgroundJobType::EmployeeSettingsExport &&
                settingsResult.success,
            "employee settings export must preserve type and result");

    std::uint64_t importRequestId = 0;
    require(queue.submitEmployeeSettingsImport(
                UiBackgroundJobOwner::AttendanceStatistics,
                importRequestId) == UiBackgroundJobSubmitError::None,
            "employee settings import must share the bounded queue");
    UiBackgroundJobResult importResult{};
    const auto importDeadline = std::chrono::steady_clock::now() +
                                std::chrono::seconds(2);
    while (!queue.tryPopResult(importResult) &&
           std::chrono::steady_clock::now() < importDeadline) {
        std::this_thread::yield();
    }
    require(settingsImportCount.load() == 1 &&
                importResult.requestId == importRequestId &&
                importResult.owner == UiBackgroundJobOwner::AttendanceStatistics &&
                importResult.type == UiBackgroundJobType::EmployeeSettingsImport &&
                importResult.success && importResult.invalidTimeCount == 3,
            "employee settings import must preserve diagnostics and result");

    std::uint64_t drainingImportRequestId = 0;
    require(queue.submitEmployeeSettingsImport(
                UiBackgroundJobOwner::AttendanceStatistics,
                drainingImportRequestId) == UiBackgroundJobSubmitError::None,
            "import accepted before stop must enter the queue");
    stopRequested.store(true);
    queue.requestStop();
    worker.join();
    UiBackgroundJobResult drainingImportResult{};
    require(queue.tryPopResult(drainingImportResult) &&
                drainingImportResult.requestId == drainingImportRequestId &&
                drainingImportResult.type ==
                    UiBackgroundJobType::EmployeeSettingsImport &&
                settingsImportCount.load() == 2,
            "stop must drain an accepted employee settings import");
    require(queue.submitUserReport(
                UiBackgroundJobOwner::RecordQuery,
                5, "2026-01-01", "2026-01-02", ignoredId) ==
                UiBackgroundJobSubmitError::Stopped,
            "stopped queue must reject new requests");

    std::cout << "[PASSED] UI background job queue is bounded and stoppable\n";
    return EXIT_SUCCESS;
}

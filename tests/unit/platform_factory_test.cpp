#include "app/platform_factory.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::filesystem::path testRoot() {
    return std::filesystem::temp_directory_path() /
        ("smartattendance_platform_test_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
}

} // namespace

int main() {
    using smart_attendance::app::PlatformConfig;
    using smart_attendance::app::PlatformInitError;
    using smart_attendance::app::createPlatformDevices;
    using smart_attendance::hal::DeviceError;

    PlatformConfig invalidDisplay;
    invalidDisplay.displayWidth = 0;
    invalidDisplay.simulatedStorageRoot = testRoot();
    const auto invalidDisplayResult = createPlatformDevices(invalidDisplay);
    require(!invalidDisplayResult &&
                invalidDisplayResult.error() ==
                    PlatformInitError::InvalidDisplaySize,
            "invalid display dimensions must be rejected");

    PlatformConfig invalidStorage;
    invalidStorage.simulatedStorageRoot = "relative/path";
    const auto invalidStorageResult = createPlatformDevices(invalidStorage);
    require(!invalidStorageResult &&
                invalidStorageResult.error() ==
                    PlatformInitError::InvalidStorageRoot,
            "relative simulated storage root must be rejected");

    const auto root = testRoot();
    PlatformConfig config;
    config.simulatedStorageRoot = root;
    auto result = createPlatformDevices(config);
    require(result && result.value().isComplete(),
            "PC factory must return a complete platform set");
    auto devices = std::move(result).value();
    require(devices.capabilities.camera &&
                devices.capabilities.display &&
                devices.capabilities.keypad &&
                devices.capabilities.systemClock &&
                !devices.capabilities.writableRtc &&
                devices.capabilities.removableStorage,
            "PC capabilities must describe the selected simulators");

    const auto now = devices.rtc->now();
    require(now && now.value().unixSeconds > 0 &&
                !devices.rtc->isWritable(),
            "PC RTC must expose readable host time without write permission");
    const auto setTime = devices.rtc->set({1000});
    require(!setTime && setTime.error() == DeviceError::Unsupported,
            "PC RTC must reject host time modification");

    const auto reportDirectory =
        devices.storage->ensureDirectory("usb_sim");
    require(reportDirectory &&
                std::filesystem::is_directory(reportDirectory.value()),
            "simulated storage must create directories below its root");
    const auto traversal = devices.storage->resolve("../escape");
    require(!traversal && traversal.error() == DeviceError::InvalidPath,
            "simulated storage must reject parent traversal");
    const auto storageSpace = devices.storage->space();
    require(storageSpace && storageSpace.value().capacityBytes > 0,
            "simulated storage must expose filesystem capacity");

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::cout << "[PASSED] PC platform factory and simulated devices\n";
    return EXIT_SUCCESS;
}

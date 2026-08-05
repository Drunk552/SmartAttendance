/**
 * @file mock_platform_devices.h
 * @brief 提供不访问宿主设备的完整平台对象集合。
 */

#ifndef SMART_ATTENDANCE_TESTS_MOCKS_MOCK_PLATFORM_DEVICES_H
#define SMART_ATTENDANCE_TESTS_MOCKS_MOCK_PLATFORM_DEVICES_H

#include "app/platform_factory.h"

#include <memory>

namespace smart_attendance::test {

class MockCamera final : public hal::ICamera {
public:
    Result<void, hal::DeviceError> open() override {
        open_ = true;
        return Result<void, hal::DeviceError>::success();
    }

    Result<hal::CameraFrame, hal::DeviceError> read() override {
        return Result<hal::CameraFrame, hal::DeviceError>::failure(
            hal::DeviceError::NoFrame);
    }

    bool isOpen() const noexcept override {
        return open_;
    }

    void close() noexcept override {
        open_ = false;
    }

private:
    bool open_{false};
};

class MockDisplay final : public hal::IDisplay {
public:
    Result<void, hal::DeviceError> initialize() override {
        return Result<void, hal::DeviceError>::success();
    }
    void shutdown() noexcept override {}
    int width() const noexcept override { return 240; }
    int height() const noexcept override { return 320; }
};

class MockKeypad final : public hal::IKeypad {
public:
    Result<void, hal::DeviceError> initialize() override {
        return Result<void, hal::DeviceError>::success();
    }
    void shutdown() noexcept override {}
};

class MockRtc final : public hal::IRtc {
public:
    Result<hal::SystemTime, hal::DeviceError> now() const override {
        return Result<hal::SystemTime, hal::DeviceError>::success({1000});
    }
    Result<void, hal::DeviceError> set(hal::SystemTime) override {
        return Result<void, hal::DeviceError>::failure(
            hal::DeviceError::Unsupported);
    }
    bool isWritable() const noexcept override { return false; }
};

class MockStorage final : public hal::IStorageDevice {
public:
    const std::filesystem::path& rootPath() const noexcept override {
        return root_;
    }
    Result<std::filesystem::path, hal::DeviceError> ensureDirectory(
        const std::filesystem::path& relativePath) override {
        return resolve(relativePath);
    }
    Result<std::filesystem::path, hal::DeviceError> resolve(
        const std::filesystem::path& relativePath) const override {
        return Result<std::filesystem::path, hal::DeviceError>::success(
            root_ / relativePath);
    }
    Result<hal::StorageSpace, hal::DeviceError> space() const override {
        return Result<hal::StorageSpace, hal::DeviceError>::success(
            {1024, 512, 512});
    }
    bool isSimulated() const noexcept override { return true; }

private:
    std::filesystem::path root_{"/tmp/smartattendance_mock_storage"};
};

inline app::PlatformDevices makeMockPlatformDevices() {
    app::PlatformDevices devices;
    devices.camera = std::make_unique<MockCamera>();
    devices.display = std::make_unique<MockDisplay>();
    devices.keypad = std::make_unique<MockKeypad>();
    devices.rtc = std::make_unique<MockRtc>();
    devices.storage = std::make_unique<MockStorage>();
    devices.capabilities = {true, true, true, true, false, true};
    return devices;
}

} // namespace smart_attendance::test

#endif // SMART_ATTENDANCE_TESTS_MOCKS_MOCK_PLATFORM_DEVICES_H

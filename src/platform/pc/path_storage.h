/**
 * @file path_storage.h
 * @brief 声明使用受控本地目录模拟的可移动存储。
 */

#ifndef SMART_ATTENDANCE_PLATFORM_PC_PATH_STORAGE_H
#define SMART_ATTENDANCE_PLATFORM_PC_PATH_STORAGE_H

#include "hal/storage_device.h"

namespace smart_attendance::platform::pc {

class PathStorage final : public hal::IStorageDevice {
public:
    explicit PathStorage(std::filesystem::path rootPath);

    const std::filesystem::path& rootPath() const noexcept override;
    Result<std::filesystem::path, hal::DeviceError> ensureDirectory(
        const std::filesystem::path& relativePath) override;
    Result<std::filesystem::path, hal::DeviceError> resolve(
        const std::filesystem::path& relativePath) const override;
    Result<hal::StorageSpace, hal::DeviceError> space() const override;
    bool isSimulated() const noexcept override;

private:
    bool isSafeRelativePath(const std::filesystem::path& path) const noexcept;

    std::filesystem::path rootPath_;
};

} // namespace smart_attendance::platform::pc

#endif // SMART_ATTENDANCE_PLATFORM_PC_PATH_STORAGE_H

/**
 * @file storage_device.h
 * @brief 定义已挂载存储目录的路径和容量能力。
 */

#ifndef SMART_ATTENDANCE_HAL_STORAGE_DEVICE_H
#define SMART_ATTENDANCE_HAL_STORAGE_DEVICE_H

#include "core/common/result.h"
#include "hal/device_error.h"

#include <cstdint>
#include <filesystem>

namespace smart_attendance::hal {

struct StorageSpace {
    std::uintmax_t capacityBytes{0};
    std::uintmax_t freeBytes{0};
    std::uintmax_t availableBytes{0};
};

/**
 * @brief 表示一个已经由系统或模拟器提供的受控存储根目录。
 * @note 本接口不负责 Linux USB 枚举、挂载或安全移除；真实 USB 管理由后续 IUsbDevice 负责。
 */
class IStorageDevice {
public:
    virtual ~IStorageDevice() = default;

    virtual const std::filesystem::path& rootPath() const noexcept = 0;
    virtual Result<std::filesystem::path, DeviceError> ensureDirectory(
        const std::filesystem::path& relativePath) = 0;
    virtual Result<std::filesystem::path, DeviceError> resolve(
        const std::filesystem::path& relativePath) const = 0;
    virtual Result<StorageSpace, DeviceError> space() const = 0;
    virtual bool isSimulated() const noexcept = 0;
};

} // namespace smart_attendance::hal

#endif // SMART_ATTENDANCE_HAL_STORAGE_DEVICE_H

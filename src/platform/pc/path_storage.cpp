/**
 * @file path_storage.cpp
 * @brief 实现限制在配置根目录内的 PC 模拟存储。
 */

#include "platform/pc/path_storage.h"

#include <system_error>
#include <utility>

namespace smart_attendance::platform::pc {

PathStorage::PathStorage(std::filesystem::path rootPath)
    : rootPath_(std::move(rootPath).lexically_normal()) {}

const std::filesystem::path& PathStorage::rootPath() const noexcept {
    return rootPath_;
}

Result<std::filesystem::path, hal::DeviceError>
PathStorage::ensureDirectory(const std::filesystem::path& relativePath) {
    using ResultType = Result<std::filesystem::path, hal::DeviceError>;
    auto resolved = resolve(relativePath);
    if (!resolved) {
        return ResultType::failure(resolved.error());
    }

    std::error_code error;
    std::filesystem::create_directories(resolved.value(), error);
    if (error) {
        return ResultType::failure(hal::DeviceError::FileSystemError);
    }
    const bool isDirectory =
        std::filesystem::is_directory(resolved.value(), error);
    if (error || !isDirectory) {
        return ResultType::failure(hal::DeviceError::FileSystemError);
    }
    return ResultType::success(resolved.value());
}

Result<std::filesystem::path, hal::DeviceError> PathStorage::resolve(
    const std::filesystem::path& relativePath) const {
    using ResultType = Result<std::filesystem::path, hal::DeviceError>;
    if (rootPath_.empty() || !rootPath_.is_absolute() ||
        !isSafeRelativePath(relativePath)) {
        return ResultType::failure(hal::DeviceError::InvalidPath);
    }
    return ResultType::success(
        (rootPath_ / relativePath).lexically_normal());
}

Result<hal::StorageSpace, hal::DeviceError> PathStorage::space() const {
    using ResultType = Result<hal::StorageSpace, hal::DeviceError>;
    std::error_code error;
    const auto info = std::filesystem::space(rootPath_, error);
    if (error) {
        return ResultType::failure(hal::DeviceError::FileSystemError);
    }
    return ResultType::success(hal::StorageSpace{
        info.capacity, info.free, info.available});
}

bool PathStorage::isSimulated() const noexcept {
    return true;
}

bool PathStorage::isSafeRelativePath(
    const std::filesystem::path& path) const noexcept {
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (const auto& component : path.lexically_normal()) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

} // namespace smart_attendance::platform::pc

/**
 * @file config_repository.h
 * @brief 声明系统配置与节假日持久化抽象。
 */

#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_CONFIG_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_CONFIG_REPOSITORY_H

#include "core/common/result.h"
#include "storage/repository/repository_error.h"

#include <optional>
#include <string>

namespace smart_attendance::storage {

class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    virtual Result<std::optional<std::string>, RepositoryError>
    findValue(const std::string& key) = 0;

    virtual Result<void, RepositoryError>
    saveValue(const std::string& key, const std::string& value) = 0;

    virtual Result<std::optional<std::string>, RepositoryError>
    findHoliday(const std::string& date) = 0;

    virtual Result<void, RepositoryError>
    saveHoliday(const std::string& date, const std::string& name) = 0;

    virtual Result<void, RepositoryError>
    deleteHoliday(const std::string& date) = 0;
};

} // namespace smart_attendance::storage

#endif // SMART_ATTENDANCE_STORAGE_REPOSITORY_CONFIG_REPOSITORY_H

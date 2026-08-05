/**
 * @file legacy_config_repository.h
 * @brief 声明旧 SQLite 配置 API 的 Repository 适配器。
 */

#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_CONFIG_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_CONFIG_REPOSITORY_H

#include "storage/repository/config_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyConfigRepository final : public IConfigRepository {
public:
    Result<std::optional<std::string>, RepositoryError>
    findValue(const std::string& key) override;

    Result<void, RepositoryError>
    saveValue(const std::string& key, const std::string& value) override;

    Result<std::optional<std::string>, RepositoryError>
    findHoliday(const std::string& date) override;

    Result<void, RepositoryError>
    saveHoliday(const std::string& date, const std::string& name) override;

    Result<void, RepositoryError>
    deleteHoliday(const std::string& date) override;
};

} // namespace smart_attendance::storage::sqlite

#endif // SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_CONFIG_REPOSITORY_H

/**
 * @file legacy_config_repository.cpp
 * @brief 将旧配置 DAO 的返回语义映射为明确 Repository 错误。
 */

#include "legacy_config_repository.h"

#include "data/db_storage.h"

#include <utility>

namespace smart_attendance::storage::sqlite {
namespace {

bool isValidText(const std::string& value) noexcept {
    return !value.empty();
}

} // namespace

Result<std::optional<std::string>, RepositoryError>
LegacyConfigRepository::findValue(const std::string& key) {
    using ResultType = Result<std::optional<std::string>, RepositoryError>;
    if (!isValidText(key)) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }

    try {
        DbTextLookupResult lookup = db_find_system_config(key);
        if (lookup.status == DbTextLookupStatus::ReadError) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }
        if (lookup.status == DbTextLookupStatus::NotFound) {
            return ResultType::success(std::nullopt);
        }
        return ResultType::success(std::move(lookup.value));
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

Result<void, RepositoryError>
LegacyConfigRepository::saveValue(const std::string& key, const std::string& value) {
    using ResultType = Result<void, RepositoryError>;
    if (!isValidText(key)) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open() || !db_set_system_config(key, value)) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<std::optional<std::string>, RepositoryError>
LegacyConfigRepository::findHoliday(const std::string& date) {
    using ResultType = Result<std::optional<std::string>, RepositoryError>;
    if (!isValidText(date)) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open()) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
    try {
        DbTextLookupResult lookup = db_find_holiday(date);
        if (lookup.status == DbTextLookupStatus::ReadError) {
            return ResultType::failure(RepositoryError::ReadFailed);
        }
        if (lookup.status == DbTextLookupStatus::NotFound) {
            return ResultType::success(std::nullopt);
        }
        return ResultType::success(std::move(lookup.value));
    } catch (...) {
        return ResultType::failure(RepositoryError::ReadFailed);
    }
}

Result<void, RepositoryError>
LegacyConfigRepository::saveHoliday(const std::string& date, const std::string& name) {
    using ResultType = Result<void, RepositoryError>;
    if (!isValidText(date) || !isValidText(name)) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open() || !db_set_holiday(date, name)) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

Result<void, RepositoryError>
LegacyConfigRepository::deleteHoliday(const std::string& date) {
    using ResultType = Result<void, RepositoryError>;
    if (!isValidText(date)) {
        return ResultType::failure(RepositoryError::InvalidArgument);
    }
    if (!data_is_open() || !db_delete_holiday(date)) {
        return ResultType::failure(RepositoryError::WriteFailed);
    }
    return ResultType::success();
}

} // namespace smart_attendance::storage::sqlite

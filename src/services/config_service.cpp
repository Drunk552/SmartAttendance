/**
 * @file config_service.cpp
 * @brief 实现公司名称配置用例。
 */

#include "config_service.h"

namespace smart_attendance::services {
namespace {

constexpr const char* kCompanyNameKey = "company_name";
constexpr const char* kUnsetCompanyName = "未设置";

} // namespace

ConfigService::ConfigService(storage::IConfigRepository& repository) noexcept
    : repository_(repository) {}

Result<std::string, ConfigError> ConfigService::loadCompanyName() {
    using ResultType = Result<std::string, ConfigError>;
    const auto result = repository_.findValue(kCompanyNameKey);
    if (!result) {
        return ResultType::failure(ConfigError::ReadFailed);
    }
    if (!result.value()) {
        return ResultType::success(kUnsetCompanyName);
    }
    return ResultType::success(*result.value());
}

Result<void, ConfigError>
ConfigService::saveCompanyName(const std::string& name) {
    using ResultType = Result<void, ConfigError>;
    if (name.empty()) {
        return ResultType::failure(ConfigError::InvalidCompanyName);
    }

    const auto result = repository_.saveValue(kCompanyNameKey, name);
    if (!result) {
        return ResultType::failure(ConfigError::WriteFailed);
    }
    return ResultType::success();
}

} // namespace smart_attendance::services

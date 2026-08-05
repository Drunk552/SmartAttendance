/**
 * @file config_service.h
 * @brief 声明系统配置用例服务。
 */

#ifndef SMART_ATTENDANCE_SERVICES_CONFIG_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_CONFIG_SERVICE_H

#include "core/common/result.h"
#include "storage/repository/config_repository.h"

#include <string>

namespace smart_attendance::services {

enum class ConfigError {
    InvalidCompanyName,
    ReadFailed,
    WriteFailed
};

/**
 * @brief 编排公司名称配置，不暴露配置键或具体存储实现。
 *
 * 本类不拥有 Repository、不创建线程。方法为同步阻塞调用，只应在低频 UI
 * 操作或受控后台任务中使用。
 */
class ConfigService final {
public:
    explicit ConfigService(storage::IConfigRepository& repository) noexcept;

    /** @return 当前公司名称；未配置时返回兼容文本“未设置”。 */
    Result<std::string, ConfigError> loadCompanyName();

    /** @return 空名称返回 InvalidCompanyName，存储失败返回 WriteFailed。 */
    Result<void, ConfigError> saveCompanyName(const std::string& name);

private:
    storage::IConfigRepository& repository_;
};

} // namespace smart_attendance::services

#endif // SMART_ATTENDANCE_SERVICES_CONFIG_SERVICE_H

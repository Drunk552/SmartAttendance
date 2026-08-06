#ifndef SMART_ATTENDANCE_SERVICES_SYSTEM_INFO_SERVICE_H
#define SMART_ATTENDANCE_SERVICES_SYSTEM_INFO_SERVICE_H

#include "storage/repository/system_info_repository.h"

namespace smart_attendance::services {

class SystemInfoService final {
public:
    explicit SystemInfoService(storage::ISystemInfoRepository& repository) noexcept
        : repository_(repository) {}
    Result<core::SystemStats, storage::RepositoryError> statistics() {
        return repository_.statistics();
    }
private:
    storage::ISystemInfoRepository& repository_;
};

} // namespace smart_attendance::services

#endif

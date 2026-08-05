#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_SYSTEM_INFO_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_SYSTEM_INFO_REPOSITORY_H

#include "core/common/result.h"
#include "data/db_storage.h"
#include "storage/repository/repository_error.h"

namespace smart_attendance::storage {

class ISystemInfoRepository {
public:
    virtual ~ISystemInfoRepository() = default;
    virtual Result<SystemStats, RepositoryError> statistics() = 0;
};

} // namespace smart_attendance::storage

#endif

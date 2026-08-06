#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SYSTEM_INFO_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_SYSTEM_INFO_REPOSITORY_H

#include "storage/repository/system_info_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacySystemInfoRepository final : public ISystemInfoRepository {
public:
    Result<core::SystemStats, RepositoryError> statistics() override;
};

} // namespace smart_attendance::storage::sqlite

#endif

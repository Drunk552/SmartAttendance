#include "legacy_system_info_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

Result<SystemStats, RepositoryError> LegacySystemInfoRepository::statistics() {
    return Result<SystemStats, RepositoryError>::success(db_get_system_stats());
}

} // namespace smart_attendance::storage::sqlite

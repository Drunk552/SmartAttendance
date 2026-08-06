#include "legacy_system_info_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

Result<core::SystemStats, RepositoryError>
LegacySystemInfoRepository::statistics() {
    const SystemStats legacy = db_get_system_stats();
    core::SystemStats stats;
    stats.totalEmployees = legacy.total_employees;
    stats.totalAdmins = legacy.total_admins;
    stats.totalFaces = legacy.total_faces;
    stats.totalFingerprints = legacy.total_fingerprints;
    stats.totalCards = legacy.total_cards;
    return Result<core::SystemStats, RepositoryError>::success(stats);
}

} // namespace smart_attendance::storage::sqlite

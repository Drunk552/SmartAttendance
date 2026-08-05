#ifndef SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_AUTHENTICATION_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_SQLITE_LEGACY_AUTHENTICATION_REPOSITORY_H

#include "storage/repository/authentication_repository.h"

namespace smart_attendance::storage::sqlite {

class LegacyAuthenticationRepository final : public IAuthenticationRepository {
public:
    Result<PasswordVerification, RepositoryError> verifyPassword(
        int employeeId, const std::string& password) override;
    Result<std::optional<std::vector<std::uint8_t>>, RepositoryError>
    fingerprintTemplate(int employeeId) override;
};

} // namespace smart_attendance::storage::sqlite

#endif

#include "legacy_authentication_repository.h"

#include "data/db_storage.h"

namespace smart_attendance::storage::sqlite {

Result<PasswordVerification, RepositoryError>
LegacyAuthenticationRepository::verifyPassword(
    int employeeId, const std::string& password) {
    if (employeeId <= 0 || password.empty()) {
        return Result<PasswordVerification, RepositoryError>::failure(
            RepositoryError::InvalidArgument);
    }
    const auto user = db_get_user_info(employeeId);
    if (!user) {
        return Result<PasswordVerification, RepositoryError>::success(
            PasswordVerification::NotFound);
    }
    if (user->password.empty()) {
        return Result<PasswordVerification, RepositoryError>::success(
            PasswordVerification::NotConfigured);
    }
    const bool matches = user->password == password ||
                         user->password == db_hash_password(password);
    return Result<PasswordVerification, RepositoryError>::success(
        matches ? PasswordVerification::Match : PasswordVerification::Mismatch);
}

Result<std::optional<std::vector<std::uint8_t>>, RepositoryError>
LegacyAuthenticationRepository::fingerprintTemplate(int employeeId) {
    if (employeeId <= 0) {
        return Result<std::optional<std::vector<std::uint8_t>>, RepositoryError>::failure(
            RepositoryError::InvalidArgument);
    }
    const auto user = db_get_user_info(employeeId);
    if (!user) {
        return Result<std::optional<std::vector<std::uint8_t>>, RepositoryError>::success(
            std::nullopt);
    }
    return Result<std::optional<std::vector<std::uint8_t>>, RepositoryError>::success(
        user->fingerprint_feature);
}

} // namespace smart_attendance::storage::sqlite

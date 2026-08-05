#include "business/auth_service.h"

#include <cassert>

namespace {

class FakeAuthenticationRepository final
    : public smart_attendance::storage::IAuthenticationRepository {
public:
    smart_attendance::Result<smart_attendance::storage::PasswordVerification,
                             smart_attendance::storage::RepositoryError>
    verifyPassword(int, const std::string&) override {
        if (passwordFails) {
            return decltype(verifyPassword(0, {}))::failure(
                smart_attendance::storage::RepositoryError::ReadFailed);
        }
        return decltype(verifyPassword(0, {}))::success(passwordResult);
    }

    smart_attendance::Result<std::optional<std::vector<std::uint8_t>>,
                             smart_attendance::storage::RepositoryError>
    fingerprintTemplate(int) override {
        if (fingerprintFails) {
            return decltype(fingerprintTemplate(0))::failure(
                smart_attendance::storage::RepositoryError::ReadFailed);
        }
        return decltype(fingerprintTemplate(0))::success(fingerprint);
    }

    smart_attendance::storage::PasswordVerification passwordResult{
        smart_attendance::storage::PasswordVerification::Match};
    std::optional<std::vector<std::uint8_t>> fingerprint{
        std::vector<std::uint8_t>{1, 2, 3}};
    bool passwordFails{false};
    bool fingerprintFails{false};
};

} // namespace

int main() {
    FakeAuthenticationRepository repository;
    AuthService service(repository);

    assert(service.verifyPassword(1, "123") == AuthResult::SUCCESS);
    repository.passwordResult =
        smart_attendance::storage::PasswordVerification::Mismatch;
    assert(service.verifyPassword(1, "bad") == AuthResult::WRONG_PASSWORD);
    repository.passwordResult =
        smart_attendance::storage::PasswordVerification::NotConfigured;
    assert(service.verifyPassword(1, "123") == AuthResult::NO_FEATURE_DATA);
    repository.passwordFails = true;
    assert(service.verifyPassword(1, "123") == AuthResult::DB_ERROR);

    assert(service.verifyFingerprint(1, {1, 2, 3}) == AuthResult::SUCCESS);
    repository.fingerprint = std::vector<std::uint8_t>{};
    assert(service.verifyFingerprint(1, {1}) == AuthResult::NO_FEATURE_DATA);
    repository.fingerprint = std::nullopt;
    assert(service.verifyFingerprint(1, {1}) == AuthResult::USER_NOT_FOUND);
    repository.fingerprintFails = true;
    assert(service.verifyFingerprint(1, {1}) == AuthResult::DB_ERROR);
    return 0;
}

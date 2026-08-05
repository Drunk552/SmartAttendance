#ifndef SMART_ATTENDANCE_STORAGE_REPOSITORY_AUTHENTICATION_REPOSITORY_H
#define SMART_ATTENDANCE_STORAGE_REPOSITORY_AUTHENTICATION_REPOSITORY_H

#include "core/common/result.h"
#include "storage/repository/employee_repository.h"
#include "storage/repository/repository_error.h"

#include <optional>
#include <cstdint>
#include <vector>

namespace smart_attendance::storage {

class IAuthenticationRepository {
public:
    virtual ~IAuthenticationRepository() = default;
    virtual Result<PasswordVerification, RepositoryError> verifyPassword(
        int employeeId, const std::string& password) = 0;
    virtual Result<std::optional<std::vector<std::uint8_t>>, RepositoryError>
    fingerprintTemplate(int employeeId) = 0;
};

} // namespace smart_attendance::storage

#endif

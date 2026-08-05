#include "services/maintenance_service.h"
#include <cstdlib>
#include <iostream>
namespace {
using namespace smart_attendance;
struct FakeRepository final : storage::IMaintenanceRepository {
    bool fail{false};
    Result<void, storage::RepositoryError> result() { return fail ? Result<void, storage::RepositoryError>::failure(storage::RepositoryError::WriteFailed) : Result<void, storage::RepositoryError>::success(); }
    Result<void, storage::RepositoryError> clearAttendance() override { return result(); }
    Result<void, storage::RepositoryError> clearEmployees() override { return result(); }
    Result<void, storage::RepositoryError> clearAllData() override { return result(); }
    Result<void, storage::RepositoryError> factoryReset() override { return result(); }
};
void require(bool value, const char* message) { if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
}
int main() {
    FakeRepository repository; services::MaintenanceService service(repository);
    require(service.clearAttendance() && service.clearEmployees() && service.clearAllData() && service.factoryReset(), "maintenance operations should map success");
    repository.fail = true;
    require(!service.clearAttendance() && !service.factoryReset(), "maintenance failures should map explicitly");
    std::cout << "maintenance_service_test: PASS\n";
}

#include "services/attendance_query_service.h"
#include <cstdlib>
#include <iostream>
namespace {
using namespace smart_attendance;
struct FakeRepository final : storage::IAttendanceQueryRepository {
    bool fail{false};
    Result<storage::AttendanceQueryPage, storage::RepositoryError> query(
        int, std::int64_t, std::int64_t, std::size_t, std::size_t offset) override {
        if (fail) return Result<storage::AttendanceQueryPage, storage::RepositoryError>::failure(storage::RepositoryError::ReadFailed);
        return Result<storage::AttendanceQueryPage, storage::RepositoryError>::success(
            {{{1, 2, "Alice", "Ops", 100, 0, ""}}, offset == 0});
    }
};
void require(bool value, const char* message) { if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
}
int main() {
    FakeRepository repository; services::AttendanceQueryService service(repository);
    require(static_cast<bool>(service.query(-1, 0, 10)), "valid range should query");
    const auto page = service.queryPage(-1, 0, 10, 1);
    require(page && page.value().pageIndex == 1 && page.value().hasPrevious,
            "paged query should preserve page state");
    require(!service.query(-1, 10, 0) && service.query(-1, 10, 0).error() == services::AttendanceQueryError::InvalidRange, "invalid range should be rejected");
    repository.fail = true;
    require(!service.query(2, 0, 10), "repository read failures should propagate");
    std::cout << "attendance_query_service_test: PASS\n";
}

#include "services/employee_service.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

using smart_attendance::Result;
using smart_attendance::core::Employee;
using smart_attendance::core::EmployeeRole;
using smart_attendance::services::EmployeeError;
using smart_attendance::services::EmployeePage;
using smart_attendance::services::EmployeeService;
using smart_attendance::storage::IEmployeeRepository;
using smart_attendance::storage::RepositoryError;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeEmployeeRepository final : IEmployeeRepository {
    Result<std::optional<Employee>, RepositoryError> result =
        Result<std::optional<Employee>, RepositoryError>::success(
            Employee{7, "Employee", 3, EmployeeRole::Regular});
    int calls{0};
    int lastEmployeeId{0};
    bool shouldThrow{false};
    Result<smart_attendance::storage::EmployeePage, RepositoryError> pageResult =
        Result<smart_attendance::storage::EmployeePage, RepositoryError>::success(
            {{{9, "PageEmployee", 4, EmployeeRole::Administrator}}, true});
    int pageCalls{0};
    std::size_t lastOffset{0};
    std::size_t lastLimit{0};
    bool pageShouldThrow{false};

    Result<std::optional<Employee>, RepositoryError>
    findById(int employeeId) override {
        ++calls;
        lastEmployeeId = employeeId;
        if (shouldThrow) {
            throw std::runtime_error("repository failure");
        }
        return result;
    }

    Result<smart_attendance::storage::EmployeePage, RepositoryError>
    listPage(std::size_t offset, std::size_t limit) override {
        ++pageCalls;
        lastOffset = offset;
        lastLimit = limit;
        if (pageShouldThrow) {
            throw std::runtime_error("page repository failure");
        }
        return pageResult;
    }
};

void testFoundEmployeeIsReturned() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    const auto result = service.findById(7);

    require(result && result.value().has_value(),
            "existing employee should be returned");
    require(result.value()->id == 7 && result.value()->name == "Employee",
            "employee model should pass through without storage details");
    require(repository.calls == 1 && repository.lastEmployeeId == 7,
            "service should issue one query for the requested employee");
}

void testMissingEmployeeRemainsSuccessful() {
    FakeEmployeeRepository repository;
    repository.result =
        Result<std::optional<Employee>, RepositoryError>::success(std::nullopt);
    EmployeeService service(repository);

    const auto result = service.findById(8);

    require(result && !result.value(),
            "missing employee should remain a successful empty result");
}

void testInvalidIdStopsBeforeRepository() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    const auto zero = service.findById(0);
    const auto negative = service.findById(-1);

    require(!zero && zero.error() == EmployeeError::InvalidEmployeeId,
            "zero employee id should be rejected");
    require(!negative && negative.error() == EmployeeError::InvalidEmployeeId,
            "negative employee id should be rejected");
    require(repository.calls == 0,
            "invalid employee ids must not access storage");
}

void testStorageFailuresAreMapped() {
    FakeEmployeeRepository repository;
    repository.result =
        Result<std::optional<Employee>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    EmployeeService service(repository);

    const auto failure = service.findById(7);
    require(!failure && failure.error() == EmployeeError::ReadFailed,
            "repository read failure should retain operation context");

    repository.shouldThrow = true;
    const auto exception = service.findById(7);
    require(!exception && exception.error() == EmployeeError::ReadFailed,
            "repository exception must not cross the service boundary");
}

void testEmployeePageIsReturned() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    const Result<EmployeePage, EmployeeError> result = service.listPage(12, 8);

    require(result && result.value().employees.size() == 1,
            "valid employee page should be returned");
    require(result.value().employees.front().id == 9 && result.value().hasMore,
            "employee page contents and continuation state should be preserved");
    require(repository.pageCalls == 1 && repository.lastOffset == 12 &&
                repository.lastLimit == 8,
            "service should forward the bounded page request exactly once");
}

void testInvalidPageStopsBeforeRepository() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    const auto zero = service.listPage(0, 0);
    const auto tooLarge = service.listPage(
        0, smart_attendance::storage::kMaxEmployeePageSize + 1);

    require(!zero && zero.error() == EmployeeError::InvalidPageRequest,
            "zero page size should be rejected");
    require(!tooLarge && tooLarge.error() == EmployeeError::InvalidPageRequest,
            "oversized page should be rejected");
    require(repository.pageCalls == 0,
            "invalid page size must not access storage");
}

void testPageFailuresAreMapped() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    repository.pageResult =
        Result<smart_attendance::storage::EmployeePage, RepositoryError>::failure(
            RepositoryError::InvalidArgument);
    auto result = service.listPage(0, 1);
    require(!result && result.error() == EmployeeError::InvalidPageRequest,
            "repository argument rejection should retain request context");

    repository.pageResult =
        Result<smart_attendance::storage::EmployeePage, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    result = service.listPage(0, 1);
    require(!result && result.error() == EmployeeError::ReadFailed,
            "repository page read failure should retain operation context");

    repository.pageShouldThrow = true;
    result = service.listPage(0, 1);
    require(!result && result.error() == EmployeeError::ReadFailed,
            "repository page exception must not cross the service boundary");
}

} // namespace

int main() {
    testFoundEmployeeIsReturned();
    testMissingEmployeeRemainsSuccessful();
    testInvalidIdStopsBeforeRepository();
    testStorageFailuresAreMapped();
    testEmployeePageIsReturned();
    testInvalidPageStopsBeforeRepository();
    testPageFailuresAreMapped();
    std::cout << "employee_service_test: PASS\n";
    return EXIT_SUCCESS;
}

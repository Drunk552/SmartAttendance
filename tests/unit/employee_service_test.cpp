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
    bool displayShouldThrow{false};
    Result<std::optional<smart_attendance::storage::EmployeeDisplayDetails>,
           RepositoryError> displayResult =
        Result<std::optional<smart_attendance::storage::EmployeeDisplayDetails>,
               RepositoryError>::success(std::nullopt);
    Result<smart_attendance::storage::EmployeePage, RepositoryError> pageResult =
        Result<smart_attendance::storage::EmployeePage, RepositoryError>::success(
            {{{{9, "PageEmployee", 4, EmployeeRole::Administrator}, "Engineering"}},
             true});
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

    Result<std::optional<smart_attendance::storage::EmployeeDisplayDetails>,
           RepositoryError>
    findDisplayDetailsById(int) override {
        if (displayShouldThrow) {
            throw std::runtime_error("display repository failure");
        }
        return displayResult;
    }

    Result<void, RepositoryError> updateName(int, const std::string&) override {
        return Result<void, RepositoryError>::success();
    }

    Result<void, RepositoryError> updateDepartment(int, int) override {
        return Result<void, RepositoryError>::success();
    }

    Result<void, RepositoryError> updateRole(int, int) override {
        return Result<void, RepositoryError>::success();
    }

    Result<smart_attendance::storage::PasswordVerification, RepositoryError>
    verifyPassword(int, const std::string&) override {
        return Result<smart_attendance::storage::PasswordVerification,
                      RepositoryError>::success(
            smart_attendance::storage::PasswordVerification::Match);
    }

    Result<bool, RepositoryError>
    updatePassword(int, const std::string&) override {
        return Result<bool, RepositoryError>::success(true);
    }

    Result<bool, RepositoryError> remove(int) override {
        return Result<bool, RepositoryError>::success(true);
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
    require(result.value().employees.front().employee.id == 9 &&
                result.value().employees.front().departmentName == "Engineering" &&
                result.value().hasMore,
            "employee page contents and continuation state should be preserved");
    require(repository.pageCalls == 1 && repository.lastOffset == 12 &&
                repository.lastLimit == 8,
            "service should forward the bounded page request exactly once");
}

void testDisplayDetailsAreMapped() {
    FakeEmployeeRepository repository;
    repository.displayResult = Result<std::optional<
        smart_attendance::storage::EmployeeDisplayDetails>, RepositoryError>::success(
        smart_attendance::storage::EmployeeDisplayDetails{
            Employee{11, "Display", 5, EmployeeRole::Regular},
            "Support", true, true, "CARD-11", false});
    EmployeeService service(repository);

    const auto result = service.findDisplayDetailsById(11);
    require(result && result.value() && result.value()->employee.id == 11 &&
                result.value()->departmentName == "Support" &&
                result.value()->faceRegistered &&
                result.value()->fingerprintRegistered &&
                result.value()->cardId == "CARD-11" &&
                !result.value()->passwordRegistered,
            "service should map non-sensitive display detail fields");

    repository.displayShouldThrow = true;
    const auto failure = service.findDisplayDetailsById(11);
    require(!failure && failure.error() == EmployeeError::ReadFailed,
            "display repository exceptions must be mapped");
}

void testBasicUpdatesValidateBeforeStorage() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    const auto invalidName = service.updateName(1, "");
    const auto invalidDepartment = service.updateDepartment(1, 0);
    const auto invalidEmployee = service.updateName(0, "Name");
    const auto invalidRole = service.updateRole(1, 2);

    require(!invalidName && invalidName.error() == EmployeeError::InvalidName,
            "empty employee names should be rejected by the service");
    require(!invalidDepartment &&
                invalidDepartment.error() == EmployeeError::InvalidDepartmentId,
            "invalid departments should be rejected by the service");
    require(!invalidEmployee &&
                invalidEmployee.error() == EmployeeError::InvalidEmployeeId,
            "invalid employee ids should be rejected by the service");
    require(!invalidRole && invalidRole.error() == EmployeeError::InvalidRole,
            "invalid employee roles should be rejected by the service");
}

void testRemoveMapsNotFoundAndInvalidId() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    require(service.remove(7).hasValue(),
            "successful employee removal should return success");
    const auto invalid = service.remove(0);
    require(!invalid && invalid.error() == EmployeeError::InvalidEmployeeId,
            "invalid employee removal id should be rejected");
}

void testPasswordValidationAndMapping() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);

    const auto verified = service.verifyPassword(7, "123456");
    require(verified &&
                verified.value() ==
                    smart_attendance::services::PasswordVerification::Match,
            "password verification result should be mapped by the service");
    require(service.updatePassword(7, "123456").hasValue(),
            "valid password updates should succeed");

    const auto empty = service.verifyPassword(7, "");
    const auto tooLong = service.updatePassword(7, "1234567");
    require(!empty && empty.error() == EmployeeError::InvalidPassword,
            "empty passwords should be rejected");
    require(!tooLong && tooLong.error() == EmployeeError::InvalidPassword,
            "passwords longer than six characters should be rejected");
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
    testDisplayDetailsAreMapped();
    testBasicUpdatesValidateBeforeStorage();
    testRemoveMapsNotFoundAndInvalidId();
    testPasswordValidationAndMapping();
    testInvalidPageStopsBeforeRepository();
    testPageFailuresAreMapped();
    std::cout << "employee_service_test: PASS\n";
    return EXIT_SUCCESS;
}

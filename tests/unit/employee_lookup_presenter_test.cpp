#include "ui/presenters/employee_lookup_presenter.h"

#include <cstdlib>
#include <iostream>
#include <optional>

namespace {

using smart_attendance::Result;
using smart_attendance::core::Employee;
using smart_attendance::core::EmployeeRole;
using smart_attendance::services::EmployeeService;
using smart_attendance::storage::IEmployeeRepository;
using smart_attendance::storage::RepositoryError;
using smart_attendance::ui::EmployeeLookupPresenter;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeEmployeeRepository final : IEmployeeRepository {
    Result<std::optional<Employee>, RepositoryError> result =
        Result<std::optional<Employee>, RepositoryError>::success(std::nullopt);
    int calls{0};

    Result<std::optional<Employee>, RepositoryError>
    findById(int) override {
        ++calls;
        return result;
    }

    Result<smart_attendance::storage::EmployeePage, RepositoryError>
    listPage(std::size_t, std::size_t) override {
        return Result<smart_attendance::storage::EmployeePage, RepositoryError>::success(
            {{}, false});
    }
};

void testLegacyRoleMapping() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    repository.result = Result<std::optional<Employee>, RepositoryError>::success(
        Employee{1, "Regular", 1, EmployeeRole::Regular});
    require(presenter.roleValueById(1) == 0,
            "regular employee should preserve legacy role value zero");

    repository.result = Result<std::optional<Employee>, RepositoryError>::success(
        Employee{2, "Administrator", 1, EmployeeRole::Administrator});
    require(presenter.roleValueById(2) == 1,
            "administrator should preserve legacy role value one");

    repository.result =
        Result<std::optional<Employee>, RepositoryError>::success(std::nullopt);
    require(presenter.roleValueById(3) == -1,
            "missing employee should preserve legacy not-found value");

    repository.result =
        Result<std::optional<Employee>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    require(presenter.roleValueById(4) == -1,
            "read failure should preserve the legacy failure value");
    require(presenter.roleValueById(0) == -1,
            "invalid employee id should preserve the legacy failure value");
}

void testLegacyExistenceMapping() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    repository.result = Result<std::optional<Employee>, RepositoryError>::success(
        Employee{1, "Employee", 1, EmployeeRole::Regular});
    require(presenter.existsById(1),
            "existing employee should preserve the legacy true value");

    repository.result =
        Result<std::optional<Employee>, RepositoryError>::success(std::nullopt);
    require(!presenter.existsById(2),
            "missing employee should preserve the legacy false value");

    repository.result =
        Result<std::optional<Employee>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    require(!presenter.existsById(3),
            "read failure should preserve the legacy false value");
    require(!presenter.existsById(0),
            "invalid employee id should preserve the legacy false value");
    require(repository.calls == 3,
            "invalid employee id must be rejected before repository access");
}

} // namespace

int main() {
    testLegacyRoleMapping();
    testLegacyExistenceMapping();
    std::cout << "employee_lookup_presenter_test: PASS\n";
    return EXIT_SUCCESS;
}

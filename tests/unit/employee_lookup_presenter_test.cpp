#include "ui/presenters/employee_lookup_presenter.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

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
    Result<std::optional<smart_attendance::storage::EmployeeDisplayDetails>,
           RepositoryError> displayResult =
        Result<std::optional<smart_attendance::storage::EmployeeDisplayDetails>,
               RepositoryError>::success(std::nullopt);
    std::vector<smart_attendance::storage::EmployeePage> pages;
    bool pageFails{false};
    int pageCalls{0};

    Result<std::optional<Employee>, RepositoryError>
    findById(int) override {
        ++calls;
        return result;
    }

    Result<std::optional<smart_attendance::storage::EmployeeDisplayDetails>,
           RepositoryError>
    findDisplayDetailsById(int) override {
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
    listPage(std::size_t, std::size_t) override {
        if (pageFails) {
            return Result<smart_attendance::storage::EmployeePage,
                          RepositoryError>::failure(RepositoryError::ReadFailed);
        }
        const std::size_t index = static_cast<std::size_t>(pageCalls++);
        if (index < pages.size()) {
            return Result<smart_attendance::storage::EmployeePage,
                          RepositoryError>::success(pages[index]);
        }
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

void testDisplayDetailMapping() {
    FakeEmployeeRepository repository;
    repository.displayResult = Result<std::optional<
        smart_attendance::storage::EmployeeDisplayDetails>, RepositoryError>::success(
        smart_attendance::storage::EmployeeDisplayDetails{
            Employee{8, "Detail", 4, EmployeeRole::Administrator},
            "Engineering", true, false, "CARD-8", true});
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    const auto item = presenter.findDisplayDetailsById(8);
    require(item && item->id == 8 && item->name == "Detail" &&
                item->departmentId == 4 && item->role == 1 &&
                item->departmentName == "Engineering" &&
                item->faceRegistered && !item->fingerprintRegistered &&
                item->cardId == "CARD-8" && item->passwordRegistered,
            "display detail should map status fields without credential contents");
    require(!presenter.findDisplayDetailsById(0),
            "invalid detail id should remain a missing result");
}

void testBasicUpdateMapping() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    require(presenter.updateName(1, "Renamed"),
            "successful name update should preserve legacy true mapping");
    require(presenter.updateDepartment(1, 2),
            "successful department update should preserve legacy true mapping");
    require(presenter.updateRole(1, 1),
            "successful role update should preserve legacy true mapping");
}

void testRemoveMapping() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    require(presenter.remove(1),
            "successful removal should preserve legacy true mapping");
    require(!presenter.remove(0),
            "invalid removal id should preserve legacy false mapping");
}

void testPasswordMapping() {
    FakeEmployeeRepository repository;
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    require(presenter.verifyPassword(1, "123456"),
            "matching passwords should preserve the legacy true mapping");
    require(presenter.updatePassword(1, "654321"),
            "successful password updates should preserve the legacy true mapping");
    require(!presenter.updatePassword(1, "1234567"),
            "invalid password updates should preserve the legacy false mapping");
}

void testEmployeeListMappingAndPagination() {
    FakeEmployeeRepository repository;
    repository.pages = {
        {{{Employee{1, "Alice", 2, EmployeeRole::Regular}, "Engineering"}},
         true},
        {{{Employee{2, "Bob", 3, EmployeeRole::Administrator}, "Operations"}},
         false}};
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    const auto items = presenter.listAll();

    require(items.size() == 2 && repository.pageCalls == 2,
            "employee list should read every bounded repository page");
    require(items[0].id == 1 && items[0].name == "Alice" &&
                items[0].departmentId == 2 &&
                items[0].departmentName == "Engineering" &&
                items[0].role == 0,
            "regular employee list fields should preserve legacy UI values");
    require(items[1].id == 2 && items[1].role == 1,
            "administrator role should preserve the legacy integer value");
}

void testEmployeeListFailureRemainsEmpty() {
    FakeEmployeeRepository repository;
    repository.pageFails = true;
    EmployeeService service(repository);
    EmployeeLookupPresenter presenter(service);

    require(presenter.listAll().empty(),
            "employee list read failure should preserve the legacy empty result");
}

} // namespace

int main() {
    testLegacyRoleMapping();
    testLegacyExistenceMapping();
    testDisplayDetailMapping();
    testBasicUpdateMapping();
    testRemoveMapping();
    testPasswordMapping();
    testEmployeeListMappingAndPagination();
    testEmployeeListFailureRemainsEmpty();
    std::cout << "employee_lookup_presenter_test: PASS\n";
    return EXIT_SUCCESS;
}

#include "ui/presenters/department_presenter.h"

#include <cstdlib>
#include <iostream>
#include <optional>

namespace {

using smart_attendance::Result;
using smart_attendance::core::Department;
using smart_attendance::core::DepartmentSchedule;
using smart_attendance::storage::IDepartmentRepository;
using smart_attendance::storage::RepositoryError;
using smart_attendance::services::DepartmentService;
using smart_attendance::ui::DepartmentPresenter;
using smart_attendance::ui::DepartmentScheduleState;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeDepartmentRepository final : IDepartmentRepository {
    Result<std::vector<Department>, RepositoryError> departments =
        Result<std::vector<Department>, RepositoryError>::success(
            {{7, "Operations", 1}});
    Result<int, RepositoryError> count =
        Result<int, RepositoryError>::success(4);
    Result<std::optional<DepartmentSchedule>, RepositoryError> schedule =
        Result<std::optional<DepartmentSchedule>, RepositoryError>::success(
            DepartmentSchedule{7, "Operations", {2, 2, 0, 2, 2, 0, 0}});
    Result<void, RepositoryError> writeResult =
        Result<void, RepositoryError>::success();

    Result<void, RepositoryError>
    addDefaultCompanyDepartment(const std::string&) override {
        return writeResult;
    }
    Result<void, RepositoryError>
    renameDepartment(int, const std::string&) override {
        return writeResult;
    }
    Result<void, RepositoryError> removeDepartment(int) override {
        return writeResult;
    }
    Result<void, RepositoryError> updateSchedule(
        int, const std::string&, const std::array<int, 7>&) override {
        return writeResult;
    }

    Result<std::vector<Department>, RepositoryError>
    listDefaultCompany() override { return departments; }
    Result<int, RepositoryError> countEmployees(int) override { return count; }
    Result<std::optional<DepartmentSchedule>, RepositoryError>
    findSchedule(int) override { return schedule; }
};

} // namespace

int main() {
    FakeDepartmentRepository repository;
    DepartmentService service(repository);
    DepartmentPresenter presenter(service);

    std::vector<smart_attendance::ui::DepartmentItem> departments;
    require(presenter.listDepartments(departments) && departments.size() == 1 &&
                departments[0].id == 7 && departments[0].name == "Operations",
            "presenter must map department DTOs without storage types");

    int count = 0;
    require(presenter.employeeCount(7, count) && count == 4,
            "presenter must map employee count");
    require(presenter.addDepartment("Quality") &&
                presenter.renameDepartment(7, "Field Operations"),
            "presenter must preserve successful department writes");
    repository.count = Result<int, RepositoryError>::success(0);
    require(presenter.removeDepartment(7),
            "presenter must preserve successful department removal");
    require(presenter.updateSchedule(
                7, "Operations", {2, 2, 0, 2, 2, 0, 0}),
            "presenter must map a seven-day schedule update");
    require(!presenter.updateSchedule(7, "Operations", {1, 2}),
            "presenter must reject schedule vectors without seven days");

    DepartmentScheduleState schedule{};
    require(presenter.loadSchedule(7, schedule) &&
                schedule.departmentName == "Operations" &&
                schedule.shiftIds[1] == 2,
            "presenter must map department schedule state");

    repository.schedule =
        Result<std::optional<DepartmentSchedule>, RepositoryError>::success(
            std::nullopt);
    require(!presenter.loadSchedule(7, schedule),
            "missing schedule must map to a failed legacy page lookup");

    repository.writeResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    require(!presenter.addDepartment("Failed") &&
                !presenter.renameDepartment(7, "Failed"),
            "presenter must map department write failures to false");
    require(!presenter.updateSchedule(
                7, "Operations", {2, 2, 0, 2, 2, 0, 0}),
            "presenter must map schedule write failures to false");

    std::cout << "department_presenter_test: PASS\n";
    return EXIT_SUCCESS;
}

#include "services/department_service.h"

#include <cstdlib>
#include <iostream>
#include <optional>

namespace {

using smart_attendance::Result;
using smart_attendance::core::Department;
using smart_attendance::core::DepartmentSchedule;
using smart_attendance::services::DepartmentError;
using smart_attendance::services::DepartmentService;
using smart_attendance::storage::IDepartmentRepository;
using smart_attendance::storage::RepositoryError;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeDepartmentRepository final : IDepartmentRepository {
    Result<std::vector<Department>, RepositoryError> departments =
        Result<std::vector<Department>, RepositoryError>::success(
            {{1, "Engineering", 1}});
    Result<int, RepositoryError> count =
        Result<int, RepositoryError>::success(3);
    Result<std::optional<DepartmentSchedule>, RepositoryError> schedule =
        Result<std::optional<DepartmentSchedule>, RepositoryError>::success(
            DepartmentSchedule{1, "Engineering", {1, 0, 1, 1, 1, 0, 0}});
    Result<void, RepositoryError> addResult =
        Result<void, RepositoryError>::success();
    Result<void, RepositoryError> renameResult =
        Result<void, RepositoryError>::success();
    Result<void, RepositoryError> removeResult =
        Result<void, RepositoryError>::success();
    Result<void, RepositoryError> scheduleWriteResult =
        Result<void, RepositoryError>::success();
    int removeCalls{0};

    Result<void, RepositoryError>
    addDefaultCompanyDepartment(const std::string&) override {
        return addResult;
    }

    Result<void, RepositoryError>
    renameDepartment(int, const std::string&) override {
        return renameResult;
    }

    Result<void, RepositoryError> removeDepartment(int) override {
        ++removeCalls;
        return removeResult;
    }

    Result<void, RepositoryError> updateSchedule(
        int, const std::string&, const std::array<int, 7>&) override {
        return scheduleWriteResult;
    }

    Result<std::vector<Department>, RepositoryError>
    listDefaultCompany() override {
        return departments;
    }

    Result<int, RepositoryError> countEmployees(int) override {
        return count;
    }

    Result<std::optional<DepartmentSchedule>, RepositoryError>
    findSchedule(int) override {
        return schedule;
    }
};

} // namespace

int main() {
    FakeDepartmentRepository repository;
    DepartmentService service(repository);

    const auto departments = service.listDepartments();
    require(departments && departments.value().size() == 1,
            "department list should preserve repository data");

    require(static_cast<bool>(service.addDepartment("Quality")),
            "valid department should be added");
    const auto invalidAdd = service.addDepartment("");
    require(!invalidAdd &&
                invalidAdd.error() == DepartmentError::InvalidDepartmentName,
            "empty department name must be rejected");
    require(static_cast<bool>(service.renameDepartment(1, "Platform")),
            "valid department should be renamed");
    const auto invalidRename = service.renameDepartment(0, "Platform");
    require(!invalidRename &&
                invalidRename.error() == DepartmentError::InvalidDepartmentId,
            "rename must reject an invalid department id");

    const auto count = service.employeeCount(1);
    require(count && count.value() == 3,
            "department employee count should be returned");
    const auto invalidCount = service.employeeCount(0);
    require(!invalidCount &&
                invalidCount.error() == DepartmentError::InvalidDepartmentId,
            "invalid department id must be rejected");

    const auto occupiedRemoval = service.removeDepartment(1);
    require(!occupiedRemoval &&
                occupiedRemoval.error() == DepartmentError::HasEmployees &&
                repository.removeCalls == 0,
            "department with employees must not be removed");
    repository.count = Result<int, RepositoryError>::success(0);
    require(static_cast<bool>(service.removeDepartment(1)) &&
                repository.removeCalls == 1,
            "empty department should be removed");
    repository.count = Result<int, RepositoryError>::failure(
        RepositoryError::ReadFailed);
    const auto countFailureRemoval = service.removeDepartment(1);
    require(!countFailureRemoval &&
                countFailureRemoval.error() == DepartmentError::ReadFailed,
            "employee count failure must stop department removal");

    repository.addResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    repository.renameResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    repository.count = Result<int, RepositoryError>::success(0);
    repository.removeResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    const auto addFailure = service.addDepartment("Failed");
    require(!addFailure && addFailure.error() == DepartmentError::WriteFailed,
            "add write failure must remain explicit");
    const auto renameFailure = service.renameDepartment(1, "Failed");
    require(!renameFailure &&
                renameFailure.error() == DepartmentError::WriteFailed,
            "rename write failure must remain explicit");
    const auto removeFailure = service.removeDepartment(1);
    require(!removeFailure &&
                removeFailure.error() == DepartmentError::WriteFailed,
            "remove write failure must remain explicit");

    const auto schedule = service.findSchedule(1);
    require(schedule && schedule.value() &&
                schedule.value()->shiftIds[0] == 1,
            "department schedule should preserve all weekday assignments");

    require(static_cast<bool>(service.updateSchedule(
                1, "Engineering", {1, 0, 1, 1, 1, 0, 0})),
            "valid department schedule should be saved");
    const auto invalidScheduleId = service.updateSchedule(
        0, "Engineering", {1, 0, 1, 1, 1, 0, 0});
    require(!invalidScheduleId &&
                invalidScheduleId.error() == DepartmentError::InvalidDepartmentId,
            "schedule update must reject an invalid department id");
    const auto invalidScheduleName = service.updateSchedule(
        1, "", {1, 0, 1, 1, 1, 0, 0});
    require(!invalidScheduleName &&
                invalidScheduleName.error() ==
                    DepartmentError::InvalidDepartmentName,
            "schedule update must reject an empty department name");
    const auto invalidShift = service.updateSchedule(
        1, "Engineering", {1, 0, 11, 1, 1, 0, 0});
    require(!invalidShift &&
                invalidShift.error() == DepartmentError::InvalidShiftId,
            "schedule update must reject shift ids outside zero to ten");
    repository.scheduleWriteResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    const auto scheduleWriteFailure = service.updateSchedule(
        1, "Engineering", {1, 0, 1, 1, 1, 0, 0});
    require(!scheduleWriteFailure &&
                scheduleWriteFailure.error() == DepartmentError::WriteFailed,
            "schedule write failure must remain explicit");

    repository.departments =
        Result<std::vector<Department>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    const auto departmentFailure = service.listDepartments();
    require(!departmentFailure &&
                departmentFailure.error() == DepartmentError::ReadFailed,
            "department read failure must remain explicit");

    std::cout << "department_service_test: PASS\n";
    return EXIT_SUCCESS;
}

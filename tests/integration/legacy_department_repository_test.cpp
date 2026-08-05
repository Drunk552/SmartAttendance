#include "data/db_storage.h"
#include "storage/sqlite/legacy_department_repository.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace {

using smart_attendance::storage::RepositoryError;
using smart_attendance::storage::sqlite::LegacyDepartmentRepository;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TemporaryDatabaseDirectory final {
public:
    TemporaryDatabaseDirectory()
        : originalDirectory_(std::filesystem::current_path()) {
        char pathTemplate[] = "/tmp/smart_attendance_department_XXXXXX";
        char* created = ::mkdtemp(pathTemplate);
        require(created != nullptr, "temporary database directory should be created");
        path_ = created;
        std::filesystem::current_path(path_);
    }

    ~TemporaryDatabaseDirectory() {
        data_close();
        std::error_code error;
        std::filesystem::current_path(originalDirectory_, error);
        std::filesystem::remove_all(path_, error);
    }

private:
    std::filesystem::path originalDirectory_;
    std::filesystem::path path_;
};

} // namespace

int main() {
    TemporaryDatabaseDirectory environment;
    require(data_init(), "temporary database should initialize");
    LegacyDepartmentRepository repository;

    const auto departments = repository.listDefaultCompany();
    require(departments && !departments.value().empty(),
            "seeded default company should expose departments");

    const int departmentId = departments.value().front().id;
    const auto count = repository.countEmployees(departmentId);
    require(count && count.value() >= 0,
            "department employee count should be readable");

    const auto schedule = repository.findSchedule(departmentId);
    require(schedule && schedule.value() &&
                schedule.value()->departmentId == departmentId,
            "department schedule should preserve department identity");

    require(static_cast<bool>(
                repository.addDefaultCompanyDepartment("Phase Seven Department")),
            "department should be added through the repository");
    const auto afterAdd = repository.listDefaultCompany();
    require(static_cast<bool>(afterAdd),
            "department list should remain readable after add");
    int addedDepartmentId = 0;
    for (const auto& department : afterAdd.value()) {
        if (department.name == "Phase Seven Department") {
            addedDepartmentId = department.id;
            break;
        }
    }
    require(addedDepartmentId > 0,
            "added department should appear in the default company list");
    require(static_cast<bool>(repository.renameDepartment(
                addedDepartmentId, "Renamed Phase Seven Department")),
            "department should be renamed through the repository");
    require(static_cast<bool>(repository.updateSchedule(
                addedDepartmentId,
                "Renamed Phase Seven Department",
                {1, 0, 1, 0, 1, 0, 0})),
            "department schedule should update in one repository operation");
    const auto savedSchedule = repository.findSchedule(addedDepartmentId);
    require(savedSchedule && savedSchedule.value() &&
                savedSchedule.value()->shiftIds[0] == 1 &&
                savedSchedule.value()->shiftIds[2] == 1,
            "saved department schedule should preserve non-holiday shifts");
    require(static_cast<bool>(repository.updateSchedule(
                addedDepartmentId,
                "Renamed Phase Seven Department",
                {0, 0, 0, 0, 0, 0, 0})),
            "holiday-only schedule should clear old assignments");
    const auto clearedSchedule = repository.findSchedule(addedDepartmentId);
    require(clearedSchedule && clearedSchedule.value() &&
                clearedSchedule.value()->shiftIds[0] == 0 &&
                clearedSchedule.value()->shiftIds[2] == 0,
            "holiday-only schedule should remove previous rows");
    require(static_cast<bool>(repository.removeDepartment(addedDepartmentId)),
            "empty department should be removed through the repository");

    const auto invalidAdd = repository.addDefaultCompanyDepartment("");
    require(!invalidAdd && invalidAdd.error() == RepositoryError::InvalidArgument,
            "empty department name should be rejected by the repository");

    const auto invalidCount = repository.countEmployees(0);
    require(!invalidCount &&
                invalidCount.error() == RepositoryError::InvalidArgument,
            "invalid department id should be rejected");

    data_close();
    const auto closedRead = repository.listDefaultCompany();
    require(!closedRead && closedRead.error() == RepositoryError::ReadFailed,
            "closed database should fail department reads explicitly");

    std::cout << "legacy_department_repository_test: PASS\n";
    return EXIT_SUCCESS;
}

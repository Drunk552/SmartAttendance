#include "services/shift_service.h"

#include <cstdlib>
#include <iostream>

namespace {
using namespace smart_attendance;
struct FakeRepository final : storage::IShiftRepository {
    std::vector<core::Shift> shifts{{1, "Standard", "08:00", "12:00", "14:00", "18:00", "", "", 0}};
    bool failRead{false};
    bool failWrite{false};
    Result<std::vector<core::Shift>, storage::RepositoryError> listAll() override {
        if (failRead) return Result<std::vector<core::Shift>, storage::RepositoryError>::failure(storage::RepositoryError::ReadFailed);
        return Result<std::vector<core::Shift>, storage::RepositoryError>::success(shifts);
    }
    Result<std::optional<core::Shift>, storage::RepositoryError> findById(int id) override {
        if (failRead) return Result<std::optional<core::Shift>, storage::RepositoryError>::failure(storage::RepositoryError::ReadFailed);
        for (const auto& shift : shifts) if (shift.id == id) return Result<std::optional<core::Shift>, storage::RepositoryError>::success(shift);
        return Result<std::optional<core::Shift>, storage::RepositoryError>::success(std::nullopt);
    }
    Result<void, storage::RepositoryError> update(const core::Shift&) override {
        return failWrite ? Result<void, storage::RepositoryError>::failure(storage::RepositoryError::WriteFailed)
                         : Result<void, storage::RepositoryError>::success();
    }
};
void require(bool condition, const char* message) { if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
}
int main() {
    FakeRepository repository;
    smart_attendance::services::ShiftService service(repository);
    require(service.listShifts() && service.findById(1), "valid shifts should be readable");
    require(!service.findById(0) && service.findById(0).error() == smart_attendance::services::ShiftError::InvalidShiftId, "invalid id should be rejected");
    require(!service.findById(9) && service.findById(9).error() == smart_attendance::services::ShiftError::NotFound, "missing shift should be explicit");
    auto shift = repository.shifts.front();
    require(static_cast<bool>(service.update(shift)), "valid shift should update");
    shift.firstStart = "25:00";
    require(!service.update(shift) && service.update(shift).error() == smart_attendance::services::ShiftError::InvalidTimeRange, "invalid clock time should be rejected");
    shift.firstStart = "12:00"; shift.firstEnd = "08:00";
    require(!service.update(shift), "same-day reversed range should be rejected");
    repository.failRead = true;
    require(!service.listShifts(), "repository read failure should propagate");
    repository.failRead = false; repository.failWrite = true;
    shift.firstStart = "08:00"; shift.firstEnd = "12:00";
    require(!service.update(shift), "repository write failure should propagate");
    std::cout << "shift_service_test: PASS\n";
}

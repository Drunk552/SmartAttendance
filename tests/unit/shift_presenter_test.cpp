#include "ui/presenters/shift_presenter.h"

#include <cstdlib>
#include <iostream>

namespace {
using namespace smart_attendance;
struct FakeRepository final : storage::IShiftRepository {
    Result<std::vector<core::Shift>, storage::RepositoryError> listAll() override { return Result<std::vector<core::Shift>, storage::RepositoryError>::success({{2, "Late", "10:00", "12:00", "", "", "", "", 0}}); }
    Result<std::optional<core::Shift>, storage::RepositoryError> findById(int id) override { return Result<std::optional<core::Shift>, storage::RepositoryError>::success(id == 2 ? std::optional<core::Shift>({2, "Late", "10:00", "12:00", "", "", "", "", 0}) : std::nullopt); }
    Result<void, storage::RepositoryError> update(const core::Shift&) override { return Result<void, storage::RepositoryError>::success(); }
};
void require(bool condition, const char* message) { if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); } }
}
int main() {
    FakeRepository repository; services::ShiftService service(repository); ui::ShiftPresenter presenter(service);
    const auto items = presenter.listAll();
    require(items.size() == 1 && items[0].id == 2, "presenter should map shift list");
    ui::ShiftItem item;
    require(presenter.findById(2, item) && item.firstStart == "10:00", "presenter should map shift details");
    require(!presenter.findById(9, item), "presenter should map missing shift to false");
    require(presenter.update(item), "presenter should preserve successful update");
    std::cout << "shift_presenter_test: PASS\n";
}

#include "ui/presenters/system_info_presenter.h"

#include <cassert>

namespace {

class FakeSystemInfoRepository final
    : public smart_attendance::storage::ISystemInfoRepository {
public:
    smart_attendance::Result<SystemStats,
                             smart_attendance::storage::RepositoryError>
    statistics() override {
        if (fail) {
            return decltype(statistics())::failure(
                smart_attendance::storage::RepositoryError::ReadFailed);
        }
        return decltype(statistics())::success(stats);
    }

    SystemStats stats{};
    bool fail{false};
};

} // namespace

int main() {
    FakeSystemInfoRepository repository;
    repository.stats.total_employees = 7;
    repository.stats.total_faces = 11;
    smart_attendance::services::SystemInfoService service(repository);
    smart_attendance::ui::SystemInfoPresenter presenter(service);

    SystemStats stats{};
    assert(presenter.statistics(stats));
    assert(stats.total_employees == 7);
    assert(stats.total_faces == 11);

    repository.fail = true;
    assert(!presenter.statistics(stats));
    return 0;
}

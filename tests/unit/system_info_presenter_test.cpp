#include "ui/presenters/system_info_presenter.h"

#include <cassert>

namespace {

class FakeSystemInfoRepository final
    : public smart_attendance::storage::ISystemInfoRepository {
public:
    smart_attendance::Result<smart_attendance::core::SystemStats,
                             smart_attendance::storage::RepositoryError>
    statistics() override {
        if (fail) {
            return decltype(statistics())::failure(
                smart_attendance::storage::RepositoryError::ReadFailed);
        }
        return decltype(statistics())::success(stats);
    }

    smart_attendance::core::SystemStats stats{};
    bool fail{false};
};

} // namespace

int main() {
    FakeSystemInfoRepository repository;
    repository.stats.totalEmployees = 7;
    repository.stats.totalFaces = 11;
    smart_attendance::services::SystemInfoService service(repository);
    smart_attendance::ui::SystemInfoPresenter presenter(service);

    smart_attendance::core::SystemStats stats{};
    assert(presenter.statistics(stats));
    assert(stats.totalEmployees == 7);
    assert(stats.totalFaces == 11);

    repository.fail = true;
    assert(!presenter.statistics(stats));
    return 0;
}

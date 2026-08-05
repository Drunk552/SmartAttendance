#include "ui/presenters/settings_presenter.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

using smart_attendance::Result;
using smart_attendance::services::ConfigService;
using smart_attendance::storage::IConfigRepository;
using smart_attendance::storage::RepositoryError;
using smart_attendance::ui::SettingsPresenter;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeConfigRepository final : IConfigRepository {
    Result<std::optional<std::string>, RepositoryError> findResult =
        Result<std::optional<std::string>, RepositoryError>::success(
            std::string("Current Company"));
    Result<void, RepositoryError> saveResult =
        Result<void, RepositoryError>::success();

    Result<std::optional<std::string>, RepositoryError>
    findValue(const std::string&) override {
        return findResult;
    }

    Result<void, RepositoryError>
    saveValue(const std::string&, const std::string&) override {
        return saveResult;
    }

    Result<std::optional<std::string>, RepositoryError>
    findHoliday(const std::string&) override {
        return Result<std::optional<std::string>, RepositoryError>::success(
            std::nullopt);
    }

    Result<void, RepositoryError>
    saveHoliday(const std::string&, const std::string&) override {
        return Result<void, RepositoryError>::success();
    }

    Result<void, RepositoryError>
    deleteHoliday(const std::string&) override {
        return Result<void, RepositoryError>::success();
    }
};

} // namespace

int main() {
    FakeConfigRepository repository;
    ConfigService service(repository);
    SettingsPresenter presenter(service);

    std::string name;
    require(presenter.loadCompanyName(name) && name == "Current Company",
            "presenter must expose the loaded company name");
    require(presenter.saveCompanyName("Updated Company"),
            "presenter must preserve successful save semantics");

    repository.findResult =
        Result<std::optional<std::string>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    repository.saveResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    require(!presenter.loadCompanyName(name),
            "presenter must map read failure to false");
    require(!presenter.saveCompanyName("Updated Company"),
            "presenter must map write failure to false");

    std::cout << "settings_presenter_test: PASS\n";
    return EXIT_SUCCESS;
}

#include "services/config_service.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

using smart_attendance::Result;
using smart_attendance::services::ConfigError;
using smart_attendance::services::ConfigService;
using smart_attendance::storage::IConfigRepository;
using smart_attendance::storage::RepositoryError;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeConfigRepository final : IConfigRepository {
    Result<std::optional<std::string>, RepositoryError> findResult =
        Result<std::optional<std::string>, RepositoryError>::success(std::nullopt);
    Result<void, RepositoryError> saveResult =
        Result<void, RepositoryError>::success();
    std::string lastKey;
    std::string lastValue;
    int findCalls{0};
    int saveCalls{0};

    Result<std::optional<std::string>, RepositoryError>
    findValue(const std::string& key) override {
        ++findCalls;
        lastKey = key;
        return findResult;
    }

    Result<void, RepositoryError>
    saveValue(const std::string& key, const std::string& value) override {
        ++saveCalls;
        lastKey = key;
        lastValue = value;
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

void testLoadCompanyName() {
    FakeConfigRepository repository;
    ConfigService service(repository);

    auto missing = service.loadCompanyName();
    require(missing && missing.value() == "未设置",
            "missing company name must preserve legacy display text");

    repository.findResult =
        Result<std::optional<std::string>, RepositoryError>::success(
            std::string("Smart Attendance"));
    auto loaded = service.loadCompanyName();
    require(loaded && loaded.value() == "Smart Attendance" &&
                repository.lastKey == "company_name",
            "company name must load through the named config key");

    repository.findResult =
        Result<std::optional<std::string>, RepositoryError>::failure(
            RepositoryError::ReadFailed);
    auto failed = service.loadCompanyName();
    require(!failed && failed.error() == ConfigError::ReadFailed,
            "repository read failure must remain explicit");
}

void testSaveCompanyName() {
    FakeConfigRepository repository;
    ConfigService service(repository);

    auto invalid = service.saveCompanyName("");
    require(!invalid && invalid.error() == ConfigError::InvalidCompanyName &&
                repository.saveCalls == 0,
            "empty company name must be rejected before storage access");

    auto saved = service.saveCompanyName("Smart Attendance");
    require(saved && repository.lastKey == "company_name" &&
                repository.lastValue == "Smart Attendance",
            "valid company name must be saved through the config repository");

    repository.saveResult = Result<void, RepositoryError>::failure(
        RepositoryError::WriteFailed);
    auto failed = service.saveCompanyName("Failed");
    require(!failed && failed.error() == ConfigError::WriteFailed,
            "repository write failure must remain explicit");
}

} // namespace

int main() {
    testLoadCompanyName();
    testSaveCompanyName();
    std::cout << "config_service_test: PASS\n";
    return EXIT_SUCCESS;
}

#include "infrastructure/logging/logger.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "smartattendance_logger_test.log";
    std::remove(path.string().c_str());
    std::remove((path.string() + ".1").c_str());

    smart_attendance::logging::configureFileSink({path.string(), 80U, 2U});
    smart_attendance::logging::write(
        smart_attendance::logging::Level::Info,
        "logger_test.cpp", "first", 0, "first line");
    smart_attendance::logging::write(
        smart_attendance::logging::Level::Error,
        "logger_test.cpp", "second", 17, "second line");
    smart_attendance::logging::disableFileSink();

    const bool currentExists = std::filesystem::exists(path);
    const bool rotatedExists = std::filesystem::exists(path.string() + ".1");
    std::remove(path.string().c_str());
    std::remove((path.string() + ".1").c_str());
    return currentExists && rotatedExists ? 0 : 1;
}

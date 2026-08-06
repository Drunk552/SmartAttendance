#include "infrastructure/logging/logger.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace smart_attendance {
namespace logging {
namespace {

std::mutex logMutex;
FileSinkOptions fileOptions;
bool fileSinkEnabled = false;

const char* levelName(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "ERROR";
}

const char* baseName(const char* path) {
    if (path == nullptr) return "unknown";
    const char* name = path;
    for (const char* current = path; *current != '\0'; ++current) {
        if (*current == '/' || *current == '\\') name = current + 1;
    }
    return name;
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&value, &local);
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return stream.str();
}

void rotateIfNeeded(std::size_t incomingBytes) {
    if (!fileSinkEnabled || fileOptions.path.empty() || fileOptions.maxFiles == 0U) return;

    std::ifstream current(fileOptions.path, std::ios::binary | std::ios::ate);
    const std::size_t size = current ? static_cast<std::size_t>(current.tellg()) : 0U;
    if (size + incomingBytes <= fileOptions.maxBytes) return;

    std::remove((fileOptions.path + "." + std::to_string(fileOptions.maxFiles)).c_str());
    for (unsigned int index = fileOptions.maxFiles; index > 1U; --index) {
        const std::string from = fileOptions.path + "." + std::to_string(index - 1U);
        const std::string to = fileOptions.path + "." + std::to_string(index);
        std::rename(from.c_str(), to.c_str());
    }
    std::rename(fileOptions.path.c_str(), (fileOptions.path + ".1").c_str());
}

} // namespace

void configureFileSink(const FileSinkOptions& options) {
    std::lock_guard<std::mutex> lock(logMutex);
    fileOptions = options;
    fileSinkEnabled = !options.path.empty() && options.maxBytes > 0U && options.maxFiles > 0U;
}

void disableFileSink() {
    std::lock_guard<std::mutex> lock(logMutex);
    fileSinkEnabled = false;
    fileOptions = FileSinkOptions{};
}

void write(Level level,
           const char* module,
           const char* operation,
           int errorCode,
           const std::string& message) {
    std::ostringstream stream;
    stream << timestamp()
           << " [" << levelName(level) << ']'
           << " [thread=" << std::this_thread::get_id() << ']'
           << " [module=" << baseName(module) << ']'
           << " [operation=" << (operation != nullptr ? operation : "unknown") << ']'
           << " [error=" << errorCode << "] "
           << message;
    if (message.empty() || message.back() != '\n') stream << '\n';
    const std::string line = stream.str();

    std::lock_guard<std::mutex> lock(logMutex);
    std::cerr << line;
    if (!fileSinkEnabled) return;

    rotateIfNeeded(line.size());
    std::ofstream file(fileOptions.path, std::ios::binary | std::ios::app);
    if (file) file << line;
}

} // namespace logging
} // namespace smart_attendance

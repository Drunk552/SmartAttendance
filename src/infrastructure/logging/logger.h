#ifndef INFRASTRUCTURE_LOGGING_LOGGER_H
#define INFRASTRUCTURE_LOGGING_LOGGER_H

#include <cstddef>
#include <cstdio>
#include <sstream>
#include <string>

namespace smart_attendance {
namespace logging {

enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

struct FileSinkOptions {
    std::string path;
    std::size_t maxBytes{1024U * 1024U};
    unsigned int maxFiles{3U};
};

void configureFileSink(const FileSinkOptions& options);
void disableFileSink();
void write(Level level,
           const char* module,
           const char* operation,
           int errorCode,
           const std::string& message);

inline void writeFormat(Level level,
                        const char* module,
                        const char* operation,
                        int errorCode,
                        const char* message) {
    write(level, module, operation, errorCode, message != nullptr ? message : "");
}

template <typename Arg, typename... Args>
void writeFormat(Level level,
                 const char* module,
                 const char* operation,
                 int errorCode,
                 const char* format,
                 Arg arg,
                 Args... args) {
    const int size = std::snprintf(nullptr, 0, format, arg, args...);
    if (size < 0) {
        write(Level::Error, module, operation, errorCode, "log format error");
        return;
    }
    if (size == 0) {
        write(level, module, operation, errorCode, "");
        return;
    }
    std::string message(static_cast<std::size_t>(size), '\0');
    std::snprintf(&message[0], message.size() + 1U, format, arg, args...);
    write(level, module, operation, errorCode, message);
}

class LogStream {
public:
    LogStream(Level level, const char* module, const char* operation, int errorCode)
        : level_(level), module_(module), operation_(operation), errorCode_(errorCode) {}

    ~LogStream() {
        write(level_, module_, operation_, errorCode_, stream_.str());
    }

    template <typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

    LogStream& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
        manipulator(stream_);
        return *this;
    }

private:
    Level level_;
    const char* module_;
    const char* operation_;
    int errorCode_;
    std::ostringstream stream_;
};

} // namespace logging
} // namespace smart_attendance

#define SA_LOG_INFO(...) \
    ::smart_attendance::logging::writeFormat(::smart_attendance::logging::Level::Info, \
        __FILE__, __func__, 0, __VA_ARGS__)
#define SA_LOG_WARN(...) \
    ::smart_attendance::logging::writeFormat(::smart_attendance::logging::Level::Warn, \
        __FILE__, __func__, 0, __VA_ARGS__)
#define SA_LOG_ERROR(...) \
    ::smart_attendance::logging::writeFormat(::smart_attendance::logging::Level::Error, \
        __FILE__, __func__, 0, __VA_ARGS__)
#define SA_LOG_INFO_STREAM() \
    ::smart_attendance::logging::LogStream(::smart_attendance::logging::Level::Info, __FILE__, __func__, 0)
#define SA_LOG_WARN_STREAM() \
    ::smart_attendance::logging::LogStream(::smart_attendance::logging::Level::Warn, __FILE__, __func__, 0)
#define SA_LOG_ERROR_STREAM() \
    ::smart_attendance::logging::LogStream(::smart_attendance::logging::Level::Error, __FILE__, __func__, 0)

#endif

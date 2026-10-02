#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Error = 3,
    Off = 4
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel level) {
        m_min_level = level;
    }

#if defined(_MSC_VER)
#define PRINTF_FORMAT_STRING _Printf_format_string_
#else
#define PRINTF_FORMAT_STRING
#endif

    template <typename... Args>
    void trace(PRINTF_FORMAT_STRING const char* fmt, Args&&... args) {
        log(LogLevel::Trace, "TRACE", fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(PRINTF_FORMAT_STRING const char* fmt, Args&&... args) {
        log(LogLevel::Debug, "DEBUG", fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(PRINTF_FORMAT_STRING const char* fmt, Args&&... args) {
        log(LogLevel::Info, "INFO ", fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(PRINTF_FORMAT_STRING const char* fmt, Args&&... args) {
        log(LogLevel::Error, "ERROR", fmt, std::forward<Args>(args)...);
    }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel m_min_level = LogLevel::Info;
    std::mutex m_mutex;

    template <typename... Args>
    void log(LogLevel level, std::string_view level_str, const char* fmt, Args&&... args) {
        if (level < m_min_level) {
            return;
        }

        // Format the message using printf-style formatting safely
        std::string formatted_msg;
        if constexpr (sizeof...(Args) > 0) {
            // Determine required buffer size
            int size = std::snprintf(nullptr, 0, fmt, args...) + 1;
            if (size > 0) {
                formatted_msg.resize(size);
                std::snprintf(&formatted_msg[0], size, fmt, args...);
                formatted_msg.pop_back(); // Remove trailing null character added by snprintf
            }
        }
        else {
            formatted_msg = fmt; // No format args, just use the string literal
        }

        // Generate timestamp with milliseconds
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm bt{};
#if defined(_MSC_VER)
        localtime_s(&bt, &in_time_t);
#else
        localtime_r(&in_time_t, &bt);
#endif

        // Thread-safe console output block
        std::lock_guard<std::mutex> lock(m_mutex);

        std::cout << "[" << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << "] [" << level_str << "] " << formatted_msg;
    }
};

// Convenient shorthand macros
#define LOG_TRACE(...) Logger::instance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::instance().info(__VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().error(__VA_ARGS__)

// In Release mode (where NDEBUG is defined), Trace and Debug compile to nothing
#if defined(NDEBUG)
#define LOG_TRACE(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#else
#define LOG_TRACE(...) Logger::instance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::instance().debug(__VA_ARGS__)
#endif
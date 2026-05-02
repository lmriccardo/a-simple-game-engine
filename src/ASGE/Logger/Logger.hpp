#pragma once

#include <atomic>
#include <iostream>
#include <ostream>
#include <mutex>
#include <string>
#include <sstream>

#include <ASGE/Time/TimeUtils.hpp>

#if defined(__clang__) || defined(__GNUC__)
    #define FUNC_NAME __FUNCTION__
#else
    #define FUNC_NAME __func__
#endif

#define LOG( lvl, ... ) \
    do { asge::logger::Logger::Instance().Log(lvl, FUNC_NAME, __VA_ARGS__); } while(0)

#define LOG_DEBUG(...) LOG(asge::logger::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) LOG(asge::logger::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARNING(...) LOG(asge::logger::LogLevel::WARNING, __VA_ARGS__)
#define LOG_ERROR(...) LOG(asge::logger::LogLevel::ERROR, __VA_ARGS__)

#define LOG_INSTANCE() asge::logger::Logger::Instance()

namespace asge::logger
{

/* Enumerates all possible logging levels */
enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

/* Prints a log level string into the input out stream */
std::ostream& operator<<( std::ostream& inOss, LogLevel inLevel ) noexcept;

class Logger
{
private:
    using atomic_ll  = std::atomic<LogLevel>;

    atomic_ll          m_Level; // The current logging level of the logger
    mutable std::mutex m_Mutex; // Mutex for console/file output contention

    Logger(): m_Level{LogLevel::INFO} {} // The constructor is private

public:
    static Logger& Instance();

    /* Set a new log level for the logger */
    void SetLogLevel( LogLevel inLevel ) noexcept;

    /**
     * @brief Log a message to console
     * 
     * Log the input message to the console if the logging level
     * is at least the current one set to the logger. This function
     * is thread-safe.
     * 
     * @param inLevel   The input level of the input message
     * @param inFn      The name of the function calling this log
     * @param inMessage The actual message to be shown on screen
     */
    template<typename ...LArgs>
    void Log( LogLevel inLevel, char const* inFunction, LArgs&& ...args) 
    const noexcept
    {
        if ( inLevel < m_Level.load() ) return;

        // Build the total message string concatenating each argument
        std::ostringstream oss;
        ( oss << ... << args );

        std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
        auto currTimestamp = time::Now();

        std::cout << "[" << inLevel << "]"
                  << "[" << time::FormatTimestamp(currTimestamp, "%H:%M:%S") << "]"
                  << "[" << inFunction << "] "
                  << oss.str()
                  << "\n";
    }
};

}
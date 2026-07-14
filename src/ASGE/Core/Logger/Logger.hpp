#pragma once

#include <atomic>
#include <iostream>
#include <ostream>
#include <mutex>
#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <utility>

#include <ASGE/Core/Time/TimeUtils.hpp>

#if defined(__clang__) || defined(__GNUC__)
    #define FUNC_NAME __FUNCTION__
#else
    #define FUNC_NAME __func__
#endif

#define LOG( lvl, ... ) \
    do { asge::logger::Logger::Instance().Log(lvl, FUNC_NAME, __VA_ARGS__); } while(0)

#define LOG_DEBUG(...)   LOG((asge::logger::LogLevel::Debug), __VA_ARGS__)
#define LOG_INFO(...)    LOG((asge::logger::LogLevel::Info), __VA_ARGS__)
#define LOG_WARNING(...) LOG((asge::logger::LogLevel::Warning), __VA_ARGS__)
#define LOG_ERROR(...)   LOG((asge::logger::LogLevel::Error), __VA_ARGS__)

#define LOG_INSTANCE() asge::logger::Logger::Instance()

namespace asge::logger
{

/* Enumerates all possible logging levels */
enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

/* Prints a log level string into the input out stream */
std::ostream& operator<<( std::ostream& inOss, LogLevel inLevel ) noexcept;

namespace _internal
{

template<typename T>
constexpr bool is_string_like_v = std::is_convertible_v<std::decay_t<T>, std::string_view>;

/**
 * @brief Streams inFmt with each "{}" replaced, in order, by one of inRest.
 *
 * Best-effort: if there are more "{}" than args, the leftover placeholders
 * are left as literal text; if there are more args than "{}", the leftover
 * args are appended after the format string is spent.
 */
template<typename ...Rest>
void WriteFormatted( std::ostringstream& oss, std::string_view inFmt, Rest&& ...inRest ) noexcept
{
    ( [&]
    {
        auto pos = inFmt.find("{}");
        if ( pos != std::string_view::npos )
        {
            oss << inFmt.substr(0, pos) << inRest;
            inFmt.remove_prefix( pos + 2 );
        }
        else
        {
            oss << inRest;
        }
    }(), ... );

    oss << inFmt;
}

/**
 * @brief Builds the log message body: if inFirst is string-like, contains
 *        "{}", and there are further args, formats via WriteFormatted;
 *        otherwise falls back to plain concatenation of every argument.
 */
template<typename First, typename ...Rest>
void BuildMessage( std::ostringstream& oss, First&& inFirst, Rest&& ...inRest ) noexcept
{
    if constexpr ( sizeof...(Rest) > 0 && is_string_like_v<First> )
    {
        std::string_view fmt( inFirst );
        if ( fmt.find("{}") != std::string_view::npos )
        {
            WriteFormatted( oss, fmt, std::forward<Rest>(inRest)... );
            return;
        }
    }

    // The extra parens around the initial term are required: a binary fold's
    // left operand must itself be a single (non-fold) expression, and
    // "oss << inFirst" is already a shift-expression, not one on its own.
    ( (oss << std::forward<First>(inFirst)) << ... << std::forward<Rest>(inRest) );
}

}

class Logger
{
private:
    using atomic_ll  = std::atomic<LogLevel>;

    atomic_ll          m_Level; // The current logging level of the logger
    mutable std::mutex m_Mutex; // Mutex for console/file output contention

    Logger(): m_Level{LogLevel::Info} {} // The constructor is private

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
     * If the first argument is string-like (const char*, std::string,
     * std::string_view, ...) and contains "{}" placeholders, the remaining
     * arguments are substituted into them in order, e.g.
     * `Log(lvl, fn, "{} of {}", 1, 2)` logs "1 of 2". Otherwise every
     * argument is simply concatenated, as before.
     *
     * @param inLevel   The input level of the input message
     * @param inFn      The name of the function calling this log
     * @param inMessage The actual message (or format string) plus any
     *                   substitution arguments
     */
    template<typename ...LArgs>
    void Log( LogLevel inLevel, char const* inFunction, LArgs&& ...args)
    const noexcept
    {
        if ( inLevel < m_Level.load() ) return;

        std::ostringstream oss;
        _internal::BuildMessage( oss, std::forward<LArgs>(args)... );

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
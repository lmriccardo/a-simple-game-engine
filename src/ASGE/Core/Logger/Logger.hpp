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
#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/Strings.hpp>

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

/* Enumerates all possible logging levels, lowest to highest severity */
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

struct LogRecord
{
    LogLevel        m_Level     { LogLevel::Info }; // Severity of the record
    str::String     m_Function  {};                 // Name of the logging function
    str::String     m_Message   {};                 // The formatted message body
    time::Timestamp m_Timestamp {};                 // When the record was logged
};

class Logger
{
private:
    using atomic_ll  = std::atomic<LogLevel>;

    atomic_ll          m_Level; // The current logging level of the logger
    mutable std::mutex m_Mutex; // Mutex for console/file output contention

    mutable signals::Signal<LogRecord const&> m_OnLog; // Signal emitting the log record

    /**
     * @brief Creates the logger at Info level.
     *
     * Private: use Instance().
     */
    Logger(): m_Level{LogLevel::Info} {}

public:
    /**
     * @brief Returns the process-wide Logger.
     *
     * A function-local static, so it is created on first use and is
     * thread-safe to initialise. The LOG_* macros all go through it.
     */
    static Logger& Instance();

    /**
     * @brief Sets the minimum level that Log() lets through.
     *
     * Messages below inLevel are dropped before any formatting happens.
     * Thread-safe (stored in an atomic with relaxed ordering).
     */
    void SetLogLevel( LogLevel inLevel ) noexcept;

    /**
     * @brief Logs a message to the console.
     *
     * Printed if inLevel is at least the logger's level; thread-safe. If the first
     * argument is string-like with "{}" placeholders, the rest are substituted in
     * order (`"{} of {}", 1, 2` logs "1 of 2"); otherwise all are concatenated.
     * inFunction names the caller, and the record is also emitted on OnLog.
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

        LogRecord record { inLevel, inFunction, oss.str(), currTimestamp };
        m_OnLog.Emit( record );
    }

    /**
     * @brief Connects an input callback to the on log signal.
     *
     * inCallback runs for every record that passes the level filter, while
     * the logger's mutex is held, so it must not log or block. Returns the
     * connection so the caller can disconnect it.
     */
    signals::Signal<LogRecord const&>::conn_type
    OnLogConnect( signals::Signal<LogRecord const&>::slot_type inCallback );
};

}
#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdint>

namespace asge::time
{

using SystemClock = std::chrono::system_clock;
using SteadyClock = std::chrono::steady_clock;
using Seconds     = std::chrono::duration<float>;
using Timestamp   = SystemClock::time_point;
using Timepoint   = SteadyClock::time_point;
using Duration    = SteadyClock::duration;

/**
 * @brief Get the current time as a chrono time point.
 *
 * Wall-clock time from the system clock; it can jump if the system time
 * changes. Use SteadyNow() to measure durations.
 */
inline Timestamp Now() noexcept { return SystemClock::now(); }

/**
 * @brief Get the current timepoint from the steady clock.
 *
 * Monotonic: never goes backwards, which makes it the right clock for
 * measuring elapsed time.
 */
inline Timepoint SteadyNow() noexcept { return SteadyClock::now(); }

/**
 * @brief Converts the input timestamp into a localtime (calendar time).
 *
 * Uses the machine's local time zone. The result is a plain std::tm value,
 * so it is safe to keep.
 */
std::tm LocalTime( Timestamp const& inTimestamp ) noexcept;

/**
 * @brief Format the input timestamp into string
 * 
 * Given an input timestamp and the formatting string
 * it returns the formatted string plus the milliseconds
 * precision like: `<format>.<ms>`. inFormat is a strftime format string
 * and the timestamp is a system clock time point, shown in local time.
 */
std::string FormatTimestamp( 
    Timestamp const& inTimestamp, char const* inFormat );

/**
 * @brief Returns the nanoseconds passed from the input timepoint.
 *
 * T is the clock type (system or steady), deduced from inStart. Compares
 * against T::now(), so inStart must come from the same clock.
 */
template<typename T>
std::int64_t NanosFrom( std::chrono::time_point<T> inStart ) noexcept
{
    auto elapsed = T::now() - inStart;
    auto nanos_d = std::chrono::duration_cast<std::chrono::nanoseconds>( elapsed );
    return nanos_d.count();
}

}
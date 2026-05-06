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

/* Get the current time as a chrono time point */
inline Timestamp Now() noexcept { return SystemClock::now(); }

/* Converts the input timestamp into a localtime (calendar time) */
std::tm LocalTime( Timestamp const& inTimestamp ) noexcept;

/**
 * @brief Format the input timestamp into string
 * 
 * Given an input timestamp and the formatting string
 * it returns the formatted string plus the milliseconds
 * precision like: `<format>.<ms>`.
 * 
 * @param inTimestamp The input timestamp (system clock time point)
 * @param inFormat The formatting string for the timestamp
 */
std::string FormatTimestamp( 
    Timestamp const& inTimestamp, char const* inFormat );

}
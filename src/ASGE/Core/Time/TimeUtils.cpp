#include "TimeUtils.hpp"

std::tm asge::time::LocalTime(Timestamp const &inTimestamp) noexcept
{
    using namespace std::chrono;

    auto currTime = system_clock::to_time_t( inTimestamp );
    std::tm resLocalTime;

#ifdef _MSC_VER
    localtime_s(&resLocalTime, &currTime);
#else
    localtime_r(&currTime, &resLocalTime);
#endif

    return resLocalTime;
}

std::string asge::time::FormatTimestamp(Timestamp const &inTimestamp, char const *inFormat)
{
    using namespace std::chrono;

    std::tm currTimeCalendar = LocalTime( inTimestamp );
    auto currSeconds = time_point_cast<seconds>( inTimestamp );
    auto currMillisec = duration_cast<milliseconds>( inTimestamp - currSeconds ).count();

    std::ostringstream outStream;
    outStream << std::put_time(&currTimeCalendar, inFormat)
              << "." << std::setw(3) << std::setfill('0')
              << currMillisec;

    return outStream.str();
}
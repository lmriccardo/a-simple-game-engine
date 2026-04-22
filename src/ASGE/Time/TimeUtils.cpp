#include "TimeUtils.hpp"

std::tm asge::time::LocalTime(Timestamp const &inTimestamp) noexcept
{
    using namespace std::chrono;

    auto currTime = system_clock::to_time_t( inTimestamp );
    std::tm resLocalTime;

#ifdef _MSC_VER
    localtime_s(&resLocalTime, &currTime);
#else
    localtime_r(&resLocalTime, &currTime);
#endif

    return resLocalTime;
}

std::string asge::time::FormatTimestamp(Timestamp const &inTimestamp, char const *inFormat)
{
    using namespace std::chrono;

    Timestamp currNow = Now();
    std::tm currTimeCalendar = LocalTime( currNow );
    auto currSeconds = time_point_cast<seconds>( currNow );
    auto currMillisec = duration_cast<milliseconds>( currNow - currSeconds ).count();

    std::ostringstream outStream;
    outStream << std::put_time(&currTimeCalendar, inFormat)
              << "." << std::setw(3) << std::setfill('0')
              << currMillisec;

    return outStream.str();
}
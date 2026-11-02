#include <ASGE/Core/Time/Time.hpp>

#include <algorithm>
#include <chrono>

#include <gtest/gtest.h>

namespace
{

using Clock = asge::time::SteadyClock;
using Ms    = std::chrono::duration<double, std::milli>;

void BusyWork( std::chrono::milliseconds inDuration )
{
    auto const end = Clock::now() + inDuration;
    while ( Clock::now() < end ) {}
}

}

TEST(TimeSystemTest, FrameLimiterNeverRunsFasterThanTheCap)
{
    auto& ts = GET_TIMESYSTEM;
    ts.SetTargetFPS( 100 ); // 10 ms per frame
    ts.Tick();              // first Tick never waits

    auto const start = Clock::now();
    for ( int i = 0; i < 10; ++i )
    {
        BusyWork( std::chrono::milliseconds( 4 ) );
        ts.Tick();
    }
    auto const total = Ms( Clock::now() - start ).count();
    ts.SetTargetFPS( 0 );

    EXPECT_GE( total, 99.0 );
}

TEST(TimeSystemTest, FrameLimiterDoesNotSleepWhenTheFrameAlreadyOverranItsBudget)
{
    // The old limiter slept a full target period inside Tick() regardless of how long the frame
    // took. Take the fastest Tick() of several so scheduler noise on a busy runner can't matter.
    auto& ts = GET_TIMESYSTEM;
    ts.SetTargetFPS( 100 ); // 10 ms per frame
    ts.Tick();

    double fastest = 1e9;
    for ( int i = 0; i < 5; ++i )
    {
        BusyWork( std::chrono::milliseconds( 15 ) ); // longer than the 10 ms budget
        auto const before = Clock::now();
        ts.Tick();
        fastest = std::min( fastest, Ms( Clock::now() - before ).count() );
    }
    ts.SetTargetFPS( 0 );

    EXPECT_LT( fastest, 5.0 );
}

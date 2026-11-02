#include <ASGE/Core/Time/Time.hpp>

#include <gtest/gtest.h>

TEST(TimeSystemTest, FrameLimiterCountsTheWholeFrameNotJustItsOwnSleep)
{
    auto& ts = GET_TIMESYSTEM;
    ts.SetTargetFPS( 100 ); // 10 ms per frame
    ts.Tick();              // first Tick never waits

    auto const start = asge::time::SteadyClock::now();
    for ( int i = 0; i < 10; ++i )
    {
        // 4 ms of "frame work": the old limiter slept a full 10 ms on top of this (14 ms/frame).
        auto const work = asge::time::SteadyClock::now() + std::chrono::milliseconds( 4 );
        while ( asge::time::SteadyClock::now() < work ) {}
        ts.Tick();
    }
    auto const total = std::chrono::duration<double, std::milli>( asge::time::SteadyClock::now() - start ).count();
    ts.SetTargetFPS( 0 );

    EXPECT_GE( total, 99.0 );  // never faster than the cap
    EXPECT_LT( total, 125.0 ); // old behaviour would be ~140 ms
}

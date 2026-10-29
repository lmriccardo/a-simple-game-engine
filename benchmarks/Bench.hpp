#pragma once

#include <algorithm>
#include <chrono>
#include <vector>

namespace bench
{

struct Stats
{
    double m_MedianNs; // Median wall time of one repetition
    double m_MinNs;    // Fastest repetition, the least noisy figure
};

/** @brief Runs inFn inWarmup times untimed, then inReps times timed, and returns median/min wall time. */
template<typename Fn>
Stats Measure( int inWarmup, int inReps, Fn&& inFn )
{
    using Clock = std::chrono::steady_clock;

    for ( int i = 0; i < inWarmup; ++i ) inFn();

    std::vector<double> samples;
    samples.reserve( static_cast<std::size_t>( inReps ) );

    for ( int i = 0; i < inReps; ++i )
    {
        auto const start = Clock::now();
        inFn();
        auto const end = Clock::now();
        samples.push_back( std::chrono::duration<double, std::nano>( end - start ).count() );
    }

    std::sort( samples.begin(), samples.end() );
    return Stats{ samples[samples.size() / 2], samples.front() };
}

}

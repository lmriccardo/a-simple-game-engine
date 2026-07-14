#include <chrono>
#include <iostream>
#include <thread>
#include <ASGE/Core/Profiling/ScopedTimer.hpp>

using namespace asge::profiling;

namespace
{

void FastOperation()
{
    PROFILE_SCOPE("FastOperation");
    std::this_thread::sleep_for( std::chrono::milliseconds(1) );
}

void SlowOperation()
{
    PROFILE_SCOPE("SlowOperation");
    std::this_thread::sleep_for( std::chrono::milliseconds(10) );
}

void FrameUpdate()
{
    // A scope can wrap other scopes, just like a real per-frame update would.
    PROFILE_SCOPE("FrameUpdate");
    FastOperation();
    SlowOperation();
}

}

int main()
{
    constexpr int frames = 5;

    for ( int frame = 0; frame < frames; ++frame )
    {
        FrameUpdate();
    }

    std::cout << "-- Summary after " << frames << " frames --" << std::endl;
    TimingProfiler::Instance().LogSummary();

    TimingProfiler::Instance().ResetAll();

    std::cout << "-- Summary after ResetAll (buckets are empty, nothing logged) --" << std::endl;
    TimingProfiler::Instance().LogSummary();

    return 0;
}

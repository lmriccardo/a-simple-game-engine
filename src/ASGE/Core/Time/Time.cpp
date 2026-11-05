#include "Time.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace asge::time;

namespace
{

/**
 * @brief Sleeps for roughly inDuration.
 *
 * On Windows std::this_thread::sleep_for rounds up to the ~15.6 ms system timer
 * tick, so a high-resolution waitable timer is used instead.
 */
void CoarseSleep( Duration inDuration ) noexcept
{
#ifdef _WIN32
    // ponytail: handle lives for the whole process and is never closed; the OS reclaims it on exit.
    static HANDLE const timer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS );
    if ( timer )
    {
        LARGE_INTEGER due;
        due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>( inDuration ).count() / 100;
        if ( SetWaitableTimer( timer, &due, 0, nullptr, nullptr, FALSE ) )
        {
            WaitForSingleObject( timer, INFINITE );
            return;
        }
    }
#endif
    std::this_thread::sleep_for( inDuration );
}

}

_internal::TimeSystem &asge::time::_internal::TimeSystem::Instance()
{
    static TimeSystem ts;
    return ts;
}

void asge::time::_internal::TimeSystem::Tick() noexcept
{
    // Frame limiter: wait until one target period has passed since the previous
    // Tick, so the sleep only covers what's left of the frame budget after the
    // caller's own Update/Render/Present work.
    if ( s_TargetFps > 0 && s_FrameIndex > 0 )
    {
        const auto deadline = s_Previous + std::chrono::duration_cast<Duration>(
            std::chrono::duration<double>( 1.0 / static_cast<double>( s_TargetFps ) ) );

        // Sleeping can overshoot by a millisecond or so, so sleep coarsely
        // and spin the last stretch.
        constexpr auto kSpinWindow = std::chrono::milliseconds( 2 );
        if ( const auto coarse = deadline - kSpinWindow; SteadyClock::now() < coarse )
            CoarseSleep( coarse - SteadyClock::now() );
        while ( SteadyClock::now() < deadline ) std::this_thread::yield();
    }

    const auto now = SteadyClock::now();

    if ( s_FrameIndex > 0 )
    {
        const auto rawDelta = std::chrono::duration_cast<Seconds>(now - s_Previous);
        s_UnscaledDelta = rawDelta.count();

        if (s_Paused) s_Delta = 0.0f;
        else {
            s_Delta = s_UnscaledDelta * s_Scale;
            s_Elapsed += s_Delta;
        }
    }

    s_Previous = now;
    ++s_FrameIndex;
}

float asge::time::_internal::TimeSystem::Delta() const noexcept
{
    return s_Delta;
}

float asge::time::_internal::TimeSystem::UnscaledDelta() const noexcept
{
    return s_UnscaledDelta;
}

float asge::time::_internal::TimeSystem::Elapsed() const noexcept
{
    return s_Elapsed;
}

std::uint64_t asge::time::_internal::TimeSystem::FrameIndex() const noexcept
{
    return s_FrameIndex;
}

float asge::time::_internal::TimeSystem::Scale() const noexcept
{
    return s_Scale;
}

bool asge::time::_internal::TimeSystem::Paused() const noexcept
{
    return s_Paused;
}

std::uint64_t asge::time::_internal::TimeSystem::TargetFPS() const noexcept
{
    return s_TargetFps;
}

void asge::time::_internal::TimeSystem::Pause(bool inPause) noexcept
{
    s_Paused = inPause;
}

void asge::time::_internal::TimeSystem::SetScale(float inScale) noexcept
{
    s_Scale = inScale < 0.0f ? 0.0f : inScale;
}

void asge::time::_internal::TimeSystem::SetTargetFPS(std::uint64_t inFps) noexcept
{
    s_TargetFps = inFps;
}

float asge::time::DeltaTime() noexcept
{
    return GET_TIMESYSTEM.Delta();
}

float asge::time::UnscaledDeltaTime() noexcept
{
    return GET_TIMESYSTEM.UnscaledDelta();
}

float asge::time::ElapsedTime() noexcept
{
    return GET_TIMESYSTEM.Elapsed();
}

std::uint64_t asge::time::FrameIndex() noexcept
{
    return GET_TIMESYSTEM.FrameIndex();
}

float asge::time::Scale() noexcept
{
    return GET_TIMESYSTEM.Scale();
}

void asge::time::Scale(float inScale) noexcept
{
    GET_TIMESYSTEM.SetScale(inScale);
}

bool asge::time::Paused() noexcept
{
    return GET_TIMESYSTEM.Paused();
}

void asge::time::Pause(bool inPause) noexcept
{
    GET_TIMESYSTEM.Pause( inPause );
}

void asge::time::TargetFPS(std::uint64_t inFps) noexcept
{
    GET_TIMESYSTEM.SetTargetFPS( inFps );
}

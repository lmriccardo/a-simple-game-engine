#include "Time.hpp"

using namespace asge::time;

_internal::TimeSystem &asge::time::_internal::TimeSystem::Instance()
{
    static TimeSystem ts;
    return ts;
}

void asge::time::_internal::TimeSystem::Tick() noexcept
{
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

    if ( s_TargetFps > 0 )
    {
        const auto target_d = 1.0 / static_cast<double>(s_TargetFps);
        const auto target = std::chrono::duration<double>(target_d);
        const auto elapsed = SteadyClock::now() - now;

        if ( elapsed < target )
        {
            std::this_thread::sleep_for(target - elapsed);
        }
    }
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

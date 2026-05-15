#pragma once

#include <thread>
#include "TimeUtils.hpp"

#define GET_TIMESYSTEM asge::time::_internal::TimeSystem::Instance()

namespace asge::time
{
 
namespace _internal
{

class TimeSystem
{
private:
    Timepoint     s_Previous{};             // Previous frame timestamp used to calculate delta time
    float         s_Delta{ 0.0f };          // Delta time after time scaling and pause handling
    float         s_UnscaledDelta{ 0.0f };  // Raw frame delta unaffected by scaling or pause
    float         s_Elapsed{0.0f};          // Total accumulated scaled runtime in seconds
    float         s_Scale{1.0f};            // Global time multiplier (0.5 = slow motion, 2.0 = fast)
    bool          s_Paused{false};          // Whether scaled time progression is paused
    std::uint64_t s_FrameIndex{0};          // Total number of processed frames since initialization
    std::uint64_t s_TargetFps{0};           // The target FPS to reach (frame limiting behavior)

    TimeSystem()
    : s_Previous(SteadyClock::now())
    {}

public:
    static TimeSystem& Instance();
    
    void Tick() noexcept;

    float         Delta() const noexcept;
    float         UnscaledDelta() const noexcept;
    float         Elapsed() const noexcept;
    std::uint64_t FrameIndex() const noexcept;
    float         Scale() const noexcept;
    bool          Paused() const noexcept;
    std::uint64_t TargetFPS() const noexcept;

    void Pause( bool inPause ) noexcept;
    void SetScale( float inScale ) noexcept;
    void SetTargetFPS( std::uint64_t inFps ) noexcept;
};

}

/**
 * @brief Get the scaled frame delta time in seconds
 * 
 * Returns the frame delta after pause and time scaling
 * have been applied.
 */
float DeltaTime() noexcept;

/**
 * Returns the frame delta without pause or time scaling.
 */
float UnscaledDeltaTime() noexcept;

/**
 * @brief Get the total accumulated scaled runtime
 * 
 * Returns the elapsed engine time in seconds after
 * applying pause and time scaling.
 */
float ElapsedTime() noexcept;

/**
 * Get the current frame index
 */
std::uint64_t FrameIndex() noexcept;

/**
 * Get the current global time scale
 */
float Scale() noexcept;

/**
 * @brief Set the global time scale multiplier
 * 
 * Values greater than 1 accelerate time,
 * while values between 0 and 1 slow it down.
 * 
 * @param inScale New time scale multiplier
 */
void Scale( float inScale ) noexcept;

/**
 * @brief Check whether time progression is paused
 * 
 * When paused, Delta() returns zero while
 * UnscaledDelta() continues updating.
 * 
 * @return True if paused
 */
bool Paused() noexcept;

/**
 * @brief Pause or resume scaled time progression
 * 
 * @param inPause True to pause, false to resume
 */
void Pause( bool inPause ) noexcept;

/* Set target FPS for frame limiting */
void TargetFPS( std::uint64_t inFps ) noexcept;

}
#pragma once

#include <thread>
#include "TimeUtils.hpp"

#define GET_TIMESYSTEM asge::time::_internal::TimeSystem::Instance()

namespace asge::time
{
 
namespace _internal
{

/**
 * @brief The process-wide frame clock behind the time:: free functions.
 *
 * Tracks per-frame delta (scaled and unscaled), total scaled time, frame
 * count, pause state and the optional FPS cap. Use the free functions in
 * asge::time instead of this class directly.
 */
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

    /**
     * @brief Starts the clock at the current time; only Instance() creates it.
     */
    TimeSystem()
    : s_Previous(SteadyClock::now())
    {}

public:
    /**
     * @brief Returns the process-wide TimeSystem (created on first use).
     */
    static TimeSystem& Instance();

    /**
     * @brief Advances one frame: applies the FPS cap, then updates the deltas.
     *
     * Call once per frame at the top of the loop. When a target FPS is set it
     * waits out the rest of the frame budget (a coarse sleep plus a short
     * spin). The first Tick only records a start time, so the deltas stay 0.
     */
    void Tick() noexcept;

    /**
     * @brief Last frame's duration in seconds after pause and scale; 0 while paused.
     */
    float         Delta() const noexcept;

    /**
     * @brief Last frame's raw duration in seconds, ignoring pause and scale.
     */
    float         UnscaledDelta() const noexcept;

    /**
     * @brief Total scaled run time in seconds; does not advance while paused.
     */
    float         Elapsed() const noexcept;

    /**
     * @brief Number of Tick() calls so far.
     */
    std::uint64_t FrameIndex() const noexcept;

    /**
     * @brief The current time scale multiplier.
     */
    float         Scale() const noexcept;

    /**
     * @brief True while scaled time is paused.
     */
    bool          Paused() const noexcept;

    /**
     * @brief The frame cap in frames per second; 0 means uncapped.
     */
    std::uint64_t TargetFPS() const noexcept;

    /**
     * @brief Pauses or resumes scaled time.
     *
     * While paused Delta() is 0 and Elapsed() stands still; UnscaledDelta()
     * keeps updating.
     */
    void Pause( bool inPause ) noexcept;

    /**
     * @brief Sets the time scale multiplier; negative values are clamped to 0.
     */
    void SetScale( float inScale ) noexcept;

    /**
     * @brief Sets the frame cap in frames per second; 0 disables the cap.
     */
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
 * @brief Returns the frame delta without pause or time scaling.
 *
 * The raw duration of the last frame, in seconds.
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
 * @brief Get the current frame index.
 *
 * Counts Tick() calls since startup, so it is 0 before the first frame.
 */
std::uint64_t FrameIndex() noexcept;

/**
 * @brief Get the current global time scale.
 *
 * 1.0 is normal speed; see Scale(float) to change it.
 */
float Scale() noexcept;

/**
 * @brief Set the global time scale multiplier
 * 
 * Values greater than 1 accelerate time,
 * while values between 0 and 1 slow it down. Negative values are clamped
 * to 0.
 */
void Scale( float inScale ) noexcept;

/**
 * @brief Check whether time progression is paused
 * 
 * When paused, Delta() returns zero while
 * UnscaledDelta() continues updating.
 */
bool Paused() noexcept;

/**
 * @brief Pause or resume scaled time progression.
 *
 * Pass true to pause and false to resume.
 */
void Pause( bool inPause ) noexcept;

/**
 * @brief Set target FPS for frame limiting.
 *
 * The limit is enforced inside the next Tick() calls; 0 turns it off.
 */
void TargetFPS( std::uint64_t inFps ) noexcept;

}
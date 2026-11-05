#pragma once

#include <algorithm>

#include "Easing.hpp"

namespace asge::math
{

/**
 * @brief Moves a value of type T (a float, a Float2, ...) from one value to another over a duration.
 *
 * The motion is shaped by an Easing. Advance it with Update(), read it with
 * Value(); it does not loop and holds no callbacks -- poll IsFinished() or
 * Update()'s result. T needs +, - and scaling by a float.
 */
template<typename T>
class Tween
{
    T      m_From;           // Value at the start
    T      m_To;             // Value at the end
    float  m_Duration;       // Length in seconds
    float  m_Elapsed{ 0.0f };// Seconds played so far, capped at m_Duration
    Easing m_Easing;         // Curve applied to the progress

public:
    /**
     * @brief Creates a tween from inFrom to inTo lasting inDuration seconds.
     *
     * A duration of 0 or less is finished at once.
     */
    Tween( T inFrom, T inTo, float inDuration, Easing inEasing = Easing::Linear ) noexcept
        : m_From( inFrom ), m_To( inTo ), m_Duration( inDuration ), m_Easing( inEasing )
    {}

    /**
     * @brief Advances by inDeltaTime seconds, stopping at the end.
     *
     * Returns true only on the call that finishes the tween; calls after that
     * (and calls on an already finished tween) return false.
     */
    bool Update( float inDeltaTime ) noexcept
    {
        if ( IsFinished() ) return false;

        m_Elapsed = std::min( m_Elapsed + inDeltaTime, m_Duration );
        return IsFinished();
    }

    /**
     * @brief The eased value at the current time.
     *
     * The start before any Update, the end once finished.
     */
    [[nodiscard]] T Value() const noexcept
    {
        if ( m_Duration <= 0.0f ) return m_To;

        float const eased = Ease( m_Easing, m_Elapsed / m_Duration );
        return T( m_From + ( m_To - m_From ) * eased );
    }

    /**
     * @brief True once the full duration has elapsed.
     *
     * Immediately true for a duration of 0 or less.
     */
    [[nodiscard]] bool IsFinished() const noexcept { return m_Elapsed >= m_Duration; }

    /**
     * @brief Rewinds to the start so the tween can play again.
     */
    void Reset() noexcept { m_Elapsed = 0.0f; }
};

}

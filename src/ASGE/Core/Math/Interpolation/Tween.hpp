#pragma once

#include <algorithm>

#include "Easing.hpp"

namespace asge::math
{

/**
 * @brief Moves a value of type T (a float, a Float2, ...) from one value to another over a
 *        duration, shaped by an Easing. Advance it with Update(), read it with Value(); it
 *        does not loop and holds no callbacks -- poll IsFinished() or Update()'s result.
 */
template<typename T>
class Tween
{
    T      m_From;
    T      m_To;
    float  m_Duration;
    float  m_Elapsed{ 0.0f };
    Easing m_Easing;

public:
    /** @brief A tween from inFrom to inTo lasting inDuration seconds; a duration of 0 or less is finished at once. */
    Tween( T inFrom, T inTo, float inDuration, Easing inEasing = Easing::Linear ) noexcept
        : m_From( inFrom ), m_To( inTo ), m_Duration( inDuration ), m_Easing( inEasing )
    {}

    /** @brief Advances by inDeltaTime seconds, stopping at the end; returns true only on the call that finishes it. */
    bool Update( float inDeltaTime ) noexcept
    {
        if ( IsFinished() ) return false;

        m_Elapsed = std::min( m_Elapsed + inDeltaTime, m_Duration );
        return IsFinished();
    }

    /** @brief The eased value at the current time: the start before any Update, the end once finished. */
    [[nodiscard]] T Value() const noexcept
    {
        if ( m_Duration <= 0.0f ) return m_To;

        float const eased = Ease( m_Easing, m_Elapsed / m_Duration );
        return T( m_From + ( m_To - m_From ) * eased );
    }

    /** @brief True once the full duration has elapsed (immediately, for a duration of 0 or less). */
    [[nodiscard]] bool IsFinished() const noexcept { return m_Elapsed >= m_Duration; }

    /** @brief Rewinds to the start so the tween can play again. */
    void Reset() noexcept { m_Elapsed = 0.0f; }
};

}

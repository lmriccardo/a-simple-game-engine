#pragma once

#include <algorithm>
#include <optional>

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/Interpolation/Tween.hpp>

namespace asge::game::resources
{

/**
 * @brief Registry-wide full-screen color overlay, drawn last by systems::ScreenFadeSystem.
 *
 * Set it with Registry::SetResource<ScreenFade>(...). Change m_Amount directly
 * with Set(), or let FadeTo() ease it there over time; the system advances the
 * tween each frame and skips the draw while m_Amount is 0.
 */
struct ScreenFade
{
    graphics::RGBA_Color              m_Color{ 0, 0, 0, 255 }; // Overlay color
    float                             m_Amount{ 0.0f };        // 0 = clear, 1 = fully covered
    std::optional<math::Tween<float>> m_Tween{};               // Running fade, if any

    /**
     * @brief Jumps to inAmount (clamped to 0..1) and cancels any running fade.
     */
    void Set( float inAmount ) noexcept
    {
        m_Amount = std::clamp( inAmount, 0.0f, 1.0f );
        m_Tween.reset();
    }

    /**
     * @brief Eases m_Amount from its current value to inTarget over inDuration seconds.
     *
     * inTarget is clamped to 0..1 and replaces any fade already running. A
     * duration of 0 or less snaps on the next system update.
     */
    void FadeTo( float inTarget, float inDuration, math::Easing inEasing = math::Easing::Linear ) noexcept
    {
        m_Tween.emplace( m_Amount, std::clamp( inTarget, 0.0f, 1.0f ), inDuration, inEasing );
    }

    /**
     * @brief True while a FadeTo has not reached its target yet.
     */
    [[nodiscard]] bool IsFading() const noexcept { return m_Tween.has_value() && !m_Tween->IsFinished(); }
};

}

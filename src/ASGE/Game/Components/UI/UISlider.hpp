#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include "Common.hpp"

namespace asge::game::components
{

/** @brief Fraction of a slider's height left empty above and below its track bar. */
static constexpr float kTrackBarSize = 0.30f;

/**
 * @brief A draggable value picker between m_Min and m_Max, drawn and hit-tested like UIButton.
 *
 * A full slider is UISlider + Interactable + UIRect (its footprint). RenderSystem
 * draws a filled/empty track split at the thumb plus a circular thumb colored by
 * m_ThumbColor's state. systems::UIInteractionSystem sets m_Value from the pointer's
 * x while the Interactable is held (even off the rect) and fires m_OnValueChanged
 * whenever it changes, including on the initial press.
 */
struct UISlider
{
    // Serialized fields
    float                m_Min          { 0.0f };
    float                m_Max          { 1.0f };
    float                m_Value        { 0.0f };
    graphics::RGBA_Color m_TrackColor   { graphics::colors::s_Gray };
    details::StateColors m_ThumbColor   {};

    // Runtime attributes
    signals::Signal<float> m_OnValueChanged; // Fires with the new value whenever dragging changes it
};

/** @brief Value units covered per pixel of a track inWidth wide, i.e. (m_Max - m_Min) / inWidth. */
float GetStep( UISlider const& inSlider, float inWidth ) noexcept;

/** @brief Thumb center inside inOrigin: m_Value mapped (clamped to the range) along its width, vertically centered. */
math::Float2 GetThumbPosition( UISlider const& inSlider, math::Rect inOrigin ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UISlider>
{
    static constexpr str::StringView kTableName = "UISlider";
    using T = components::UISlider;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
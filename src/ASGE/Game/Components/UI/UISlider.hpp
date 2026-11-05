#pragma once

#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include "Common.hpp"

namespace asge::game::components
{

/**
 * @brief Fraction of a slider's height left empty above and below its track bar.
 *
 * Shared by RenderSystem and the hit-test code so they agree on the track.
 */
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
    float                m_Min          { 0.0f };                       // Value at the left end
    float                m_Max          { 1.0f };                       // Value at the right end
    float                m_Value        { 0.0f };                       // Current value
    graphics::RGBA_Color m_TrackColor   { graphics::colors::s_Gray };   // Color of the unfilled track
    details::StateColors m_ThumbColor   {};                             // Thumb colors for normal, hover and pressed

    // Runtime attributes
    signals::Signal<float> m_OnValueChanged; // Fires with the new value whenever dragging changes it
};

/**
 * @brief Value units covered per pixel of a track inWidth wide.
 *
 * That is (m_Max - m_Min) / inWidth.
 */
float GetStep( UISlider const& inSlider, float inWidth ) noexcept;

/**
 * @brief Thumb center inside inOrigin.
 *
 * The slider's m_Value is mapped (clamped to the range) along the rect's
 * width and the thumb is vertically centered.
 */
math::Float2 GetThumbPosition( UISlider const& inSlider, math::Rect inOrigin ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UISlider>
{
    static constexpr str::StringView kTableName = "UISlider"; // Name of the TOML subtable (de)serialization uses
    using T = components::UISlider;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
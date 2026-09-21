#pragma once

#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief A clickable rectangular UI widget with a text label.
 *
 * m_Size is authored in the same units as Transform; the owning entity's
 * Transform positions the button (top-left, same convention as Sprite).
 * m_Hovered/m_PressedThisFrame are recomputed every frame by whatever
 * system drives UI input — not something a scene file describes — and
 * m_OnClick is fired by that same system, once, the frame the button
 * transitions from pressed to released while still hovered.
 */
struct UIButton
{
    str::String    m_FontVirtualPath{};                     // VFS path of the font m_Text is drawn with
    str::String    m_Text           {"Click Me"};            // Label drawn on the button
    str::TextAlign m_TextAlignment  {str::TextAlign::None};  // How m_Text is justified within m_Size
    math::Float2   m_Size           {80.0f, 24.0f};          // Button extent, same units/origin as Transform

    bool m_Hovered          {false}; // Whether the pointer is currently over the button -- runtime-only, never serialized
    bool m_PressedThisFrame {false}; // Whether the pointer is currently held down on the button -- runtime-only, never serialized

    signals::Signal<> m_OnClick; // Fired on click by whatever system drives UI input; connect via Signal::Connect
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_FontVirtualPath/m_Text/m_TextAlignment/m_Size only —
 *        see AudioSource's Serializer doc comment for why a scene file
 *        describes what a button looks like, not its live hover/press
 *        state or click subscribers. FromToml leaves m_Hovered/
 *        m_PressedThisFrame/m_OnClick at UIButton's in-code defaults.
 */
template<>
struct Serializer<components::UIButton>
{
    static constexpr str::StringView kTableName = "UIButton";

    using T = components::UIButton;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
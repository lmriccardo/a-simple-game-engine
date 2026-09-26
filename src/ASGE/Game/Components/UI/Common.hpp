#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

namespace details
{

/** @brief The three fill colors a widget cycles through by its sibling Interactable's m_Hovered/m_Held -- embedded in UIButton, not a standalone component. */
struct StateColors
{
    graphics::RGBA_Color m_Color        { graphics::colors::s_ButtonLight };
    graphics::RGBA_Color m_HoverColor   { graphics::colors::s_ButtonLightHover };
    graphics::RGBA_Color m_PressedColor { graphics::colors::s_ButtonLightPressed };
};

}

/**
 * @brief A rectangular footprint for a UI widget.
 *
 * m_Size is authored in the same units as Transform; the owning entity's
 * Transform positions it (top-left, same convention as Sprite). Purely
 * geometric -- RenderSystem reads it to compute a draw/hit-test rect (see
 * RectFromSize), but drawing a fill color or reacting to the pointer needs
 * a sibling UIButton/Interactable too.
 */
struct UIRect
{
    // Serialized
    math::Float2 m_Size{80.0f, 24.0f};
};

/**
 * @brief Generic pointer-interaction state for a UI widget with a UIRect footprint.
 *
 * m_Hovered/m_Held/m_Clicked are recomputed every frame by
 * systems::UIInteractionSystem from resources::UIHitList (see their own doc
 * comments) -- not something a scene file describes. m_Clicked is
 * edge-triggered: true only on the frame a press completes (released while
 * still hovered), false every other frame. m_Enabled opts a widget out of
 * hit-testing entirely (and forces m_Hovered/m_Held false) without removing
 * the component, e.g. to gray out a button.
 */
struct Interactable
{
    // Serialized
    bool m_Enabled{true};

    // Runtime
    bool m_Hovered  { false };
    bool m_Held     { false };
    bool m_Clicked  { false };
};

}

namespace asge::game::scene
{

/** @brief Round-trips m_Color/m_HoverColor/m_PressedColor as three nested RGBA_Color tables -- embedded inline (no kTableName), not registered as its own component. */
template<>
struct Serializer<components::details::StateColors>
{
    using T = components::details::StateColors;
    
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

/** @brief Round-trips UIRect::m_Size only. */
template<>
struct Serializer<components::UIRect>
{
    static constexpr str::StringView kTableName = "UIRect";

    using T = components::UIRect;
    
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

/** @brief Round-trips Interactable::m_Enabled only -- FromToml leaves m_Hovered/m_Held/m_Clicked at Interactable's in-code defaults, same reasoning as UIButton's Serializer doc comment. */
template<>
struct Serializer<components::Interactable>
{
    static constexpr str::StringView kTableName = "Interactable";

    using T = components::Interactable;
    
    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
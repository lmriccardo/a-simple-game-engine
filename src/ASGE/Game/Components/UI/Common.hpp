#pragma once

#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

namespace details
{

/**
 * @brief The three fill colors a widget cycles through by its sibling Interactable.
 *
 * Chosen by Interactable's m_Hovered/m_Held. Embedded in UIButton and the
 * other widgets, not a standalone component.
 */
struct StateColors
{
    graphics::RGBA_Color m_Color        { graphics::colors::s_ButtonLight };        // Normal fill
    graphics::RGBA_Color m_HoverColor   { graphics::colors::s_ButtonLightHover };   // Fill while the pointer is over it
    graphics::RGBA_Color m_PressedColor { graphics::colors::s_ButtonLightPressed }; // Fill while held down
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
    math::Float2 m_Size{80.0f, 24.0f}; // Width and height of the footprint
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
    bool m_Enabled{true}; // Whether the widget reacts to the pointer at all

    // Runtime
    bool m_Hovered  { false }; // The pointer is over the widget
    bool m_Held     { false }; // The pointer is pressed down on the widget
    bool m_Clicked  { false }; // True only on the frame a press completes over the widget
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::details::StateColors>
{
    using T = components::details::StateColors;
    
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

template<>
struct Serializer<components::UIRect>
{
    static constexpr str::StringView kTableName = "UIRect"; // Name of the TOML subtable (de)serialization uses

    using T = components::UIRect;
    
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

template<>
struct Serializer<components::Interactable>
{
    static constexpr str::StringView kTableName = "Interactable"; // Name of the TOML subtable (de)serialization uses

    using T = components::Interactable;
    
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
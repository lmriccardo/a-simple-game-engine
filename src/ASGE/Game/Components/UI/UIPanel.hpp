#pragma once

#include <variant>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Which LayoutSpec alternative a panel uses.
 *
 * Also its serialized m_LayoutType name.
 */
enum class PanelLayout
{
    Absolute,   // Elements gets absolute positining inside the panel
    Grid,       // Elements are positioned in grid
    VStack,     // Vertical Stacking of elements
    HStack      // Horizontal Stacking of elements
};

// Children keep the position they were given.
struct LayoutAbsolute {};

// Children fill a m_Rows x m_Cols grid of equal slots.
struct LayoutGrid{ int m_Rows{3}; int m_Cols{3}; };

// Children are stacked top to bottom in m_Rows equal slots.
struct LayoutVStack { int m_Rows{3}; };

// Children are stacked left to right in m_Cols equal slots.
struct LayoutHStack { int m_Cols{3}; };

/**
 * @brief The panel's layout mode together with that mode's parameters.
 *
 * Use GetPanelLayout() to get just the mode.
 */
using LayoutSpec =std::variant<LayoutAbsolute, LayoutGrid, LayoutVStack, LayoutHStack>;

// Maps a layout struct to its PanelLayout tag; unknown types map to Absolute.
template<typename T> struct LayoutType { static constexpr PanelLayout value = PanelLayout::Absolute; };
template<> struct LayoutType<LayoutGrid> { static constexpr PanelLayout value = PanelLayout::Grid; };
template<> struct LayoutType<LayoutVStack> { static constexpr PanelLayout value = PanelLayout::VStack; };
template<> struct LayoutType<LayoutHStack> { static constexpr PanelLayout value = PanelLayout::HStack; };

// Shorthand for LayoutType<T>::value.
template<typename T>
inline constexpr PanelLayout LayoutType_v = LayoutType<T>::value;

/**
 * @brief A rectangular background container, drawn from a sibling UIRect.
 *
 * RenderSystem fills the UIRect inset by m_Margin with m_Background, then, if
 * m_Border is set, outlines the full UIRect in m_BorderColor. systems::
 * UILayoutSystem then places and resizes its Hierarchy children into m_Layout's
 * slots, inset by m_Margin + m_Padding with m_Spacing between neighbouring slots.
 */
struct UIPanel
{
    // Serialized Fields
    graphics::RGBA_Color    m_Background   { graphics::colors::s_ShadowBlack }; // Fill color
    LayoutSpec              m_Layout       { LayoutAbsolute{} };                // How children are placed
    math::Float2            m_Padding      { 0.0f, 0.0f };                      // Inner gap before the children (x, y)
    math::Float2            m_Margin       { 0.0f, 0.0f };                      // Outer gap before the fill (x, y)
    math::Float2            m_Spacing      { 0.0f, 0.0f };                      // Gap between neighbouring slots (x, y)
    bool                    m_Border       { true };                            // Whether the outline is drawn
    graphics::RGBA_Color    m_BorderColor  { graphics::colors::s_LightGray };   // Outline color
};

/**
 * @brief Returns the PanelLayout tag of the panel's current m_Layout.
 *
 * Handy for serializing or switching on the mode without visiting the variant.
 */
PanelLayout GetPanelLayout( UIPanel const& inPanel ) noexcept;

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UIPanel>
{
    static constexpr str::StringView kTableName = "UIPanel"; // Name of the TOML subtable (de)serialization uses
    using T = components::UIPanel;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
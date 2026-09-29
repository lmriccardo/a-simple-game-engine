#pragma once

#include <variant>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/** @brief Which LayoutSpec alternative a panel uses; also its serialized m_LayoutType name. */
enum class PanelLayout
{
    Absolute,   // Elements gets absolute positining inside the panel
    Grid,       // Elements are positioned in grid
    VStack,     // Vertical Stacking of elements
    HStack      // Horizontal Stacking of elements
};

struct LayoutAbsolute {};
struct LayoutGrid{ int m_Rows{3}; int m_Cols{3}; };
struct LayoutVStack { int m_Rows{3}; };
struct LayoutHStack { int m_Cols{3}; };

/** @brief The panel's layout mode together with that mode's parameters. */
using LayoutSpec =std::variant<LayoutAbsolute, LayoutGrid, LayoutVStack, LayoutHStack>;

template<typename T> struct LayoutType { static constexpr PanelLayout value = PanelLayout::Absolute; };
template<> struct LayoutType<LayoutGrid> { static constexpr PanelLayout value = PanelLayout::Grid; };
template<> struct LayoutType<LayoutVStack> { static constexpr PanelLayout value = PanelLayout::VStack; };
template<> struct LayoutType<LayoutHStack> { static constexpr PanelLayout value = PanelLayout::HStack; };

template<typename T>
inline constexpr PanelLayout LayoutType_v = LayoutType<T>::value;

/**
 * @brief A rectangular background container, drawn from a sibling UIRect.
 *
 * RenderSystem fills the UIRect inset by m_Margin with m_Background, then, if
 * m_Border is set, outlines the full UIRect in m_BorderColor. m_Layout and
 * m_Padding are stored and serialized but nothing lays children out yet.
 */
struct UIPanel
{
    // Serialized Fields
    graphics::RGBA_Color    m_Background   { graphics::colors::s_ShadowBlack };
    LayoutSpec              m_Layout       { LayoutAbsolute{} };
    math::Float2            m_Padding      { 0.0f, 0.0f };
    math::Float2            m_Margin       { 0.0f, 0.0f };
    bool                    m_Border       { true };
    graphics::RGBA_Color    m_BorderColor  { graphics::colors::s_LightGray };
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UIPanel>
{
    static constexpr str::StringView kTableName = "UIPanel";
    using T = components::UIPanel;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
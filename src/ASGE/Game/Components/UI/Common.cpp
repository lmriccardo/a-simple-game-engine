#include "Common.hpp"

void asge::game::scene::Serializer<asge::game::components::details::StateColors>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Set( "m_Color",        static_cast<std::int64_t>(graphics::RGBATo32A(inValue.m_Color)) )
           .Set( "m_HoverColor",   static_cast<std::int64_t>(graphics::RGBATo32A(inValue.m_HoverColor)) )
           .Set( "m_PressedColor", static_cast<std::int64_t>(graphics::RGBATo32A(inValue.m_PressedColor)) );
}

asge::game::components::details::StateColors
asge::game::scene::Serializer<asge::game::components::details::StateColors>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    components::details::StateColors result;

    auto const dColor  = static_cast<std::int64_t>( graphics::RGBATo32A(result.m_Color) );
    auto const dHColor = static_cast<std::int64_t>( graphics::RGBATo32A(result.m_HoverColor) );
    auto const dPColor = static_cast<std::int64_t>( graphics::RGBATo32A(result.m_PressedColor) );

    result.m_Color        = graphics::C32AToRGBA( static_cast<graphics::Color32A>(inTview.Get( "m_Color",        dColor )) );
    result.m_HoverColor   = graphics::C32AToRGBA( static_cast<graphics::Color32A>(inTview.Get( "m_HoverColor",   dHColor )) );
    result.m_PressedColor = graphics::C32AToRGBA( static_cast<graphics::Color32A>(inTview.Get( "m_PressedColor", dPColor )) );

    return result;
}

void asge::game::scene::Serializer<asge::game::components::UIRect>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(str::String( kTableName ))
           .Set( "m_SizeX", inValue.m_Size.x() )
           .Set( "m_SizeY", inValue.m_Size.y() );
}

asge::game::components::UIRect
asge::game::scene::Serializer<asge::game::components::UIRect>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );
    components::UIRect result;
    result.m_Size = math::Float2{
        table.Get( "m_SizeX", result.m_Size.x() ), table.Get( "m_SizeY", result.m_Size.y() ),
    };

    return result;
}

void asge::game::scene::Serializer<asge::game::components::Interactable>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table(str::String( kTableName )).Set( "m_Enabled", inValue.m_Enabled );
}

asge::game::components::Interactable
asge::game::scene::Serializer<asge::game::components::Interactable>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    components::Interactable result;
    result.m_Enabled = inTview.Table( str::String(kTableName) ).Get( "m_Enabled", result.m_Enabled );
    return result;
}
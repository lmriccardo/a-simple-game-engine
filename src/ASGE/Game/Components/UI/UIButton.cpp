#include "UIButton.hpp"

void asge::game::scene::Serializer<asge::game::components::UIButton>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    table.Set( "m_FontVirtualPath", inValue.m_FontVirtualPath );
    table.Set( "m_FontWeight", inValue.m_FontWeight );
    table.Set( "m_Text", inValue.m_Text );
    table.Set( "m_TextAlign", str::ToString( inValue.m_TextAlignment ) );
    table.Set( "m_SizeX", inValue.m_Size.x() );
    table.Set( "m_SizeY", inValue.m_Size.y() );

    Serializer<graphics::RGBA_Color>::ToToml( inValue.m_Color,          table.Table( "Color" ) );
    Serializer<graphics::RGBA_Color>::ToToml( inValue.m_HoverColor,     table.Table( "HoverColor" ) );
    Serializer<graphics::RGBA_Color>::ToToml( inValue.m_PressedColor,   table.Table( "PressedColor" ) );
}

asge::game::components::UIButton
asge::game::scene::Serializer<asge::game::components::UIButton>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UIButton result;
    result.m_FontVirtualPath = table.Get( "m_FontVirtualPath", str::String{} );
    result.m_FontWeight = table.Get( "m_FontWeight", result.m_FontWeight );
    result.m_Text = table.Get( "m_Text", result.m_Text );

    auto defaultAlign = str::ToString( str::TextAlign::None );
    result.m_TextAlignment = str::FromString( table.Get( "m_TextAlign", defaultAlign ) );
    
    result.m_Size = math::Float2{
        table.Get( "m_SizeX", result.m_Size.x() ),
        table.Get( "m_SizeY", result.m_Size.y() )
    };

    result.m_Color          = Serializer<graphics::RGBA_Color>::FromToml(table.Table( "Color" ) );
    result.m_HoverColor     = Serializer<graphics::RGBA_Color>::FromToml(table.Table( "HoverColor" ) );
    result.m_PressedColor   = Serializer<graphics::RGBA_Color>::FromToml(table.Table( "PressedColor" ) );

    return result;
}

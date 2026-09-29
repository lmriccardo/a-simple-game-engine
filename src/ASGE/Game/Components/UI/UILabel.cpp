#include "UILabel.hpp"

asge::str::String asge::game::components::ToString( VerticalAlign inAlign ) noexcept
{
    switch ( inAlign )
    {
    case VerticalAlign::Top:    return "top";
    case VerticalAlign::Center: return "center";
    case VerticalAlign::Bottom: return "bottom";
    }
    return "top";
}

asge::game::components::VerticalAlign
asge::game::components::FromString( str::StringView inStr ) noexcept
{
    if ( inStr == "top" )    return VerticalAlign::Top;
    if ( inStr == "center" ) return VerticalAlign::Center;
    if ( inStr == "bottom" ) return VerticalAlign::Bottom;
    return VerticalAlign::Top;
}

void asge::game::scene::Serializer<asge::game::components::UILabel>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String(kTableName) )
           .Set("m_Text", inValue.m_Text)
           .Set("m_FontPath", inValue.m_FontPath)
           .Set("m_Align", str::ToString( inValue.m_Align ))
           .Set("m_VerticalAlign", components::ToString( inValue.m_VerticalAlign ))
           .Set("m_FontColor", static_cast<std::int64_t>(graphics::RGBATo32A( inValue.m_Color )))
           .Set("m_FontPixelHeight", inValue.m_FontPixelHeight)
           .Set("m_AutoSize", inValue.m_AutoSize);
}

asge::game::components::UILabel
asge::game::scene::Serializer<asge::game::components::UILabel>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String(kTableName) );

    components::UILabel result;

    result.m_FontPath = table.Get("m_FontPath", result.m_FontPath);
    result.m_Align = str::FromString( table.Get( "m_Align", str::ToString(result.m_Align) ) );
    result.m_VerticalAlign = components::FromString(
        table.Get( "m_VerticalAlign", components::ToString(result.m_VerticalAlign) ) );
    result.m_Text = table.Get( "m_Text", result.m_Text );
    result.m_FontPixelHeight = table.Get( "m_FontPixelHeight", result.m_FontPixelHeight );
    result.m_AutoSize = table.Get( "m_AutoSize", result.m_AutoSize );

    auto const dColor = static_cast<std::int64_t>( graphics::RGBATo32A( result.m_Color ) );
    result.m_Color = graphics::C32AToRGBA( static_cast<graphics::Color32A>( table.Get( "m_FontColor", dColor ) ) );

    return result;
}
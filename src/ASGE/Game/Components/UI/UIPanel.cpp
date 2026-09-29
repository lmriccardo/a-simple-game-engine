#include "UIPanel.hpp"

namespace
{

using namespace asge::game::components;

asge::str::String ToString(PanelLayout inValue) noexcept
{
    switch ( inValue )
    {
    case PanelLayout::Absolute: return "Absolute";
    case PanelLayout::Grid: return "Grid";
    case PanelLayout::HStack: return "HStack";
    case PanelLayout::VStack: return "VStack";
    }

    return "Absolute";
}

PanelLayout FromString(asge::str::String inValue) noexcept
{
    if ( inValue == "Absolute" ) return PanelLayout::Absolute;
    if ( inValue == "Grid" ) return PanelLayout::Grid;
    if ( inValue == "HStack" ) return PanelLayout::HStack;
    if ( inValue == "VStack" ) return PanelLayout::VStack;
    return PanelLayout::Absolute;
}

void SerializeLayout( LayoutAbsolute inLayout, asge::config::toml::TOMLTableView inTview ) {}

void SerializeLayout( LayoutGrid inLayout, asge::config::toml::TOMLTableView inTview )
{
    inTview.Set( "m_Rows", inLayout.m_Rows).Set( "m_Cols", inLayout.m_Cols );
}

void SerializeLayout( LayoutHStack inLayout, asge::config::toml::TOMLTableView inTview )
{
    inTview.Set( "m_Cols", inLayout.m_Cols);
}

void SerializeLayout( LayoutVStack inLayout, asge::config::toml::TOMLTableView inTview )
{
    inTview.Set( "m_Rows", inLayout.m_Rows);
}

void SerializeLayoutSpec( LayoutSpec inSpec, asge::config::toml::TOMLTableView inTview )
{
    std::visit( [&]( auto spec ) {
        inTview.Set( "m_LayoutType", ToString( LayoutType_v<decltype(spec)> ) );
        SerializeLayout( spec, inTview.Table( "Layout" ) ); 
    }, inSpec );
}

}

void asge::game::scene::Serializer<asge::game::components::UIPanel>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    table.Set( "m_Background", static_cast<std::int64_t>( graphics::RGBATo32A( inValue.m_Background ) ) )
         .Set( "m_PaddingX", inValue.m_Padding.x() )
         .Set( "m_PaddingY", inValue.m_Padding.y() )
         .Set( "m_MarginX", inValue.m_Margin.x() )
         .Set( "m_MarginY", inValue.m_Margin.y() )
         .Set( "m_Border", inValue.m_Border )
         .Set( "m_BorderColor", static_cast<std::int64_t>( graphics::RGBATo32A( inValue.m_BorderColor ) ) );

    SerializeLayoutSpec( inValue.m_Layout, table );
}

asge::game::components::UIPanel
asge::game::scene::Serializer<asge::game::components::UIPanel>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::UIPanel result;

    auto bColor = static_cast<std::int64_t>( graphics::RGBATo32A( result.m_Background ) );
    result.m_Background = graphics::C32AToRGBA( static_cast<graphics::Color32A>( table.Get("m_Background", bColor) ) );

    bColor = static_cast<std::int64_t>( graphics::RGBATo32A( result.m_BorderColor ) );
    result.m_BorderColor = graphics::C32AToRGBA( static_cast<graphics::Color32A>( table.Get("m_BorderColor", bColor) ) );
   
    result.m_Padding = math::Float2{
        table.Get( "m_PaddingX", result.m_Padding.x() ), table.Get( "m_PaddingY", result.m_Padding.y() )
    };

    result.m_Margin = math::Float2{
        table.Get( "m_MarginX", result.m_Margin.x() ), table.Get( "m_MarginY", result.m_Margin.y() )
    };

    result.m_Border = table.Get( "m_Border", result.m_Border );

    PanelLayout layoutT = FromString( table.Get( "m_LayoutType", str::String{"Absolute"} ) );
    auto layoutTable = table.Table( "Layout" );

    switch ( layoutT )
    {
    case PanelLayout::Absolute: result.m_Layout = LayoutAbsolute{}; break;
    case PanelLayout::Grid:
    {
        result.m_Layout = LayoutGrid{
            .m_Rows = layoutTable.Get( "m_Rows", LayoutGrid{}.m_Rows ),
            .m_Cols = layoutTable.Get( "m_Cols", LayoutGrid{}.m_Cols )
        };
        break;
    }
    case PanelLayout::HStack:
        result.m_Layout = LayoutHStack{ layoutTable.Get( "m_Cols", LayoutHStack{}.m_Cols ) };
        break;
    case PanelLayout::VStack:
        result.m_Layout = LayoutVStack{ layoutTable.Get( "m_Rows", LayoutVStack{}.m_Rows ) };
        break;
    }

    return result;
}

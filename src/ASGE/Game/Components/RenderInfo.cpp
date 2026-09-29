#include "RenderInfo.hpp"

void asge::game::scene::Serializer<asge::game::components::RenderInfo>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String( kTableName ) )
           .Set( "m_Layer", inValue.m_Layer )
           .Set( "m_YSort", inValue.m_YSort )
           .Set( "m_ScreenSpace", inValue.m_ScreenSpace )
           .Set( "m_InheritSortFromParent", inValue.m_InheritSortFromParent )
           .Set( "m_LocalOrder", inValue.m_LocalOrder );
}

asge::game::components::RenderInfo
asge::game::scene::Serializer<asge::game::components::RenderInfo>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    components::RenderInfo result;
    result.m_Layer = table.Get( "m_Layer", result.m_Layer );
    result.m_YSort = table.Get( "m_YSort", result.m_YSort );
    result.m_ScreenSpace = table.Get( "m_ScreenSpace", false );
    result.m_InheritSortFromParent = table.Get( "m_InheritSortFromParent", false );
    result.m_LocalOrder = table.Get( "m_LocalOrder", int{0} );
    return result;
}
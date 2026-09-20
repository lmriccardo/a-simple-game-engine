#include "Camera.hpp"

void asge::game::scene::Serializer<asge::game::components::Camera>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String( kTableName ) )
           .Set( "m_Zoom", inValue.m_Zoom )
           .Set( "m_Smoothing", inValue.m_Smoothing );
}

asge::game::components::Camera
asge::game::scene::Serializer<asge::game::components::Camera>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );
    T result;
    result.m_Zoom = table.Get( "m_Zoom", result.m_Zoom );
    result.m_Smoothing = table.Get( "m_Smoothing", result.m_Smoothing );
    return result;
}
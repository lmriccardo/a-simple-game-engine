#include "PathFollow.hpp"

bool asge::game::components::PathFollowPause( ecs::Registry& inRegistry, ecs::Entity inEntity ) noexcept
{
    auto follow = inRegistry.GetComponent<PathFollow>( inEntity );
    if ( !follow ) return false;

    follow.Value().get().m_Paused = true;
    return true;
}

bool asge::game::components::PathFollowResume( ecs::Registry& inRegistry, ecs::Entity inEntity ) noexcept
{
    auto follow = inRegistry.GetComponent<PathFollow>( inEntity );
    if ( !follow ) return false;

    follow.Value().get().m_Paused = false;
    return true;
}

void asge::game::scene::Serializer<asge::game::components::PathFollow>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String( kTableName ) )
           .Set( "m_Speed", inValue.m_Speed )
           .Set( "m_OffsetH", inValue.m_OffsetH )
           .Set( "m_OffsetV", inValue.m_OffsetV )
           .Set( "m_AlignRotation", inValue.m_AlignRotation );
}

asge::game::components::PathFollow
asge::game::scene::Serializer<asge::game::components::PathFollow>::FromToml(
    asge::config::toml::TOMLTableView inTview, LoadContext const& inCtx ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    T result;
    
    result.m_Speed = table.Get( "m_Speed", result.m_Speed );
    result.m_OffsetH = table.Get( "m_OffsetH", result.m_OffsetH );
    result.m_OffsetV = table.Get( "m_OffsetV", result.m_OffsetV );
    result.m_AlignRotation = table.Get( "m_AlignRotation", result.m_AlignRotation );

    return result;
}
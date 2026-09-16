#include "PathFollow.hpp"

void asge::game::components::Serializer<asge::game::components::PathFollow>::ToToml(
    T inValue, asge::config::toml::TOMLTableView inTview ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    std::vector<float> xs, ys;
    xs.reserve( inValue.m_Waypoints.size() );
    ys.reserve( inValue.m_Waypoints.size() );
    for ( auto const& pos : inValue.m_Waypoints )
    {
        xs.push_back( pos.x() );
        ys.push_back( pos.y() );
    }

    table.SetArray( "m_WaypointX", xs );
    table.SetArray( "m_WaypointY", ys );
    table.Set( "m_Speed", inValue.m_Speed );
    table.Set( "m_Loop", inValue.m_Loop );
}

asge::game::components::PathFollow 
asge::game::components::Serializer<asge::game::components::PathFollow>::FromToml( 
    asge::config::toml::TOMLTableView inTview ) noexcept
{
    auto table = inTview.Table( str::String( kTableName ) );

    PathFollow result;
    result.m_Speed = table.Get( "m_Speed", result.m_Speed );
    result.m_Loop  = table.Get( "m_Loop",  result.m_Loop  );

    std::vector<float> xs, ys;
    xs = table.GetArray( "m_WaypointX", std::vector<float>{} );
    ys = table.GetArray( "m_WaypointY", std::vector<float>{} );

    result.m_Waypoints.reserve( xs.size() );
    for ( std::size_t ii = 0; ii < xs.size(); ++ii )
    {
        result.m_Waypoints.push_back( std::move( math::Float2{ xs[ii], ys[ii] } ) );
    }

    return result;
}
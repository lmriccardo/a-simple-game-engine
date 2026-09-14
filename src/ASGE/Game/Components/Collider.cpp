#include "Collider.hpp"

asge::str::String asge::game::components::details::ToString( ResolutionType inResolveT ) noexcept
{
    switch ( inResolveT )
    {
    case ResolutionType::Solid   : return "Solid";
    case ResolutionType::Trigger : return "Trigger";
    case ResolutionType::Unknown : return "Unknown";
    }
    return "Unknown";
}

asge::game::components::ResolutionType asge::game::components::details::FromString( str::StringView inSv ) noexcept
{
    if ( inSv == "Solid" ) return ResolutionType::Solid;
    if ( inSv == "Trigger" ) return ResolutionType::Trigger;
    return ResolutionType::Unknown;
}

bool asge::game::components::details::LayersCanCollide( Collider const& inA, Collider const& inB ) noexcept
{
    return ( inA.m_Layer & inB.m_Mask ) != 0 && ( inB.m_Layer & inA.m_Mask ) != 0;
}

void asge::game::components::Serializer<asge::game::components::Collider>::ToToml(
    Collider inCollider, asge::config::toml::TOMLTableView inTview ) noexcept
{
    auto table = inTview.Table(std::string(kTableName));
    std::visit( [&table]( auto const& inShape )
    {
        using ShapeT = std::decay_t<decltype( inShape )>;
        table.Set<str::String>( "m_Shape", str::String(Serializer<ShapeT>::kShapeName) );
        Serializer<ShapeT>::ToToml( inShape, table );
    }, inCollider.m_LocalBounds);

    table.Set<str::String>( "m_Resolution", details::ToString( inCollider.m_Resolution ) );
    table.Set<int>( "m_Layer", static_cast<int>( inCollider.m_Layer ) );
    table.Set<int>( "m_Mask",  static_cast<int>( inCollider.m_Mask ) );
}

asge::game::components::Collider asge::game::components::Serializer<asge::game::components::Collider>::FromToml(
    asge::config::toml::TOMLTableView inEnttView ) noexcept
{
    auto table = inEnttView.Table(std::string(kTableName));
    // Defaults to "Rect" so a scene file saved before Circle existed --
    // no "m_Shape" key at all -- still parses as a Rect, unchanged.
    auto shapeKind = table.Get("m_Shape", std::string("Rect"));

    Collider result{};

    if ( shapeKind == Serializer<math::Rect>::kShapeName )
    {
        result.m_LocalBounds = Serializer<math::Rect>::FromToml( table );
    }
    else
    {
        result.m_LocalBounds = Serializer<math::Circle>::FromToml( table );
    }

    result.m_Resolution = details::FromString( table.Get( "m_Resolution", std::string("Solid") ) );
    result.m_Layer = static_cast<CollisionLayer>( table.Get<int>("m_Layer", 1) );
    result.m_Mask  = static_cast<CollisionLayer>( table.Get<int>("m_Mask", -1) );

    return result;
}

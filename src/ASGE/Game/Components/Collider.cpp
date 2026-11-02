#include "Collider.hpp"

namespace
{

using namespace asge;

asge::str::String ToString( asge::game::components::ResolutionType inResolveT ) noexcept
{
    switch ( inResolveT )
    {
    case asge::game::components::ResolutionType::Solid   : return "Solid";
    case asge::game::components::ResolutionType::Trigger : return "Trigger";
    case asge::game::components::ResolutionType::Unknown : return "Unknown";
    }
    return "Unknown";
}

asge::game::components::ResolutionType FromString( asge::str::StringView inSv ) noexcept
{
    if ( inSv == "Solid" ) return asge::game::components::ResolutionType::Solid;
    if ( inSv == "Trigger" ) return asge::game::components::ResolutionType::Trigger;
    return asge::game::components::ResolutionType::Unknown;
}

}

bool asge::game::components::LayersCanCollide( Collider const& inA, Collider const& inB ) noexcept
{
    return ( inA.m_Layer & inB.m_Mask ) != 0 && ( inB.m_Layer & inA.m_Mask ) != 0;
}

bool asge::game::components::HasUnknownResolution(Collider const &inCollider) noexcept
{
    return inCollider.m_Resolution == ResolutionType::Unknown;
}

void asge::game::scene::Serializer<asge::game::components::Collider>::ToToml(
    components::Collider inCollider, asge::config::toml::TOMLTableView inTview, SaveContext const& inCtx ) noexcept
{
    auto table = inTview.Table(std::string(kTableName));
    std::visit( [&table]( auto const& inShape )
    {
        using ShapeT = std::decay_t<decltype( inShape )>;
        table.Set<str::String>( "m_Shape", str::String(Serializer<ShapeT>::kShapeName) );
        Serializer<ShapeT>::ToToml( inShape, table );
    }, inCollider.m_LocalBounds);

    table.Set<str::String>( "m_Resolution", ToString( inCollider.m_Resolution ) );
    table.Set<int>( "m_Layer", static_cast<int>( inCollider.m_Layer ) );
    table.Set<int>( "m_Mask",  static_cast<int>( inCollider.m_Mask ) );
}

asge::game::components::Collider asge::game::scene::Serializer<asge::game::components::Collider>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table(std::string(kTableName));
    // Defaults to "Rect" so a scene file saved before Circle existed --
    // no "m_Shape" key at all -- still parses as a Rect, unchanged.
    auto shapeKind = table.Get("m_Shape", std::string("Rect"));

    components::Collider result{};

    if ( shapeKind == Serializer<math::Rect>::kShapeName )
    {
        result.m_LocalBounds = Serializer<math::Rect>::FromToml( table );
    }
    else
    {
        result.m_LocalBounds = Serializer<math::Circle>::FromToml( table );
    }

    result.m_Resolution = FromString( table.Get( "m_Resolution", std::string("Solid") ) );
    result.m_Layer = static_cast<components::CollisionLayer>( table.Get<int>("m_Layer", 1) );
    result.m_Mask  = static_cast<components::CollisionLayer>( table.Get<int>("m_Mask", -1) );

    return result;
}

asge::signals::Signal<asge::ecs::Entity> &asge::game::components::Collider::OnEnter() noexcept
{
    return m_OnEnter.Get();
}

asge::signals::Signal<asge::ecs::Entity> &asge::game::components::Collider::OnExit() noexcept
{
    return m_OnExit.Get();
}

asge::signals::Signal<asge::ecs::Entity> &asge::game::components::Collider::OnStay() noexcept
{
    return m_OnStay.Get();
}

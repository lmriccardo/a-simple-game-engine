#include "PhysicsSystem.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Velocity.hpp"
#include "../Components/Collider.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Events.hpp"

#include <ASGE/Core/Math/Geometry/Collision.hpp>

namespace
{
using namespace asge::game::components;
using namespace asge::ecs;
using namespace asge::math;

/** @brief inC's local shape offset by inT's position, in world space — same shape kind, new coordinates. */
ColliderShape WorldBounds( Transform const& inT, Collider const& inC ) noexcept
{
    return std::visit([&inT]( auto const& inShape ) -> ColliderShape
    {
        using ShapeT = std::decay_t<decltype(inShape)>;

        if constexpr ( std::is_same_v<ShapeT, asge::math::Rect> )
        {
            return asge::math::Rect{
                inT.m_X + inShape.x, inT.m_Y + inShape.y,
                inShape.w, inShape.h
            };
        } else {
            return asge::math::Circle{
                asge::math::Float2{ 
                    inT.m_X + inShape.m_Center.x(), inT.m_Y + inShape.m_Center.y() 
                },
                inShape.m_Radius
            };
        }
    }, inC.m_LocalBounds);
}

// Applies a positional correction and zeroes velocity on whichever axis moved.
void ApplyCorrection( Transform& inT, Velocity& inV, asge::math::Float2 const& inDelta ) noexcept
{
    inT.m_X += inDelta.x();
    inT.m_Y += inDelta.y();
    if ( inDelta.x() != 0.0f ) inV.m_DX = 0.0f;
    if ( inDelta.y() != 0.0f ) inV.m_DY = 0.0f;
}

constexpr float kGravity = 980.0f; // pixel/s^2

/**
 * @brief Pushes e1/e2 apart along mtv, splitting the correction by mass
 *        ratio when both are movable — the actual push-out logic for a
 *        Solid/Solid overlap. See ResolveCollisions for the movable
 *        rules (needs both Velocity and Rigidbody) this assumes were
 *        already checked to decide it should even be called.
 */
void ResolveSolidCollision(
    Registry &inRegistry, Entity e1, Transform& t1, Entity e2, Transform& t2, 
    Float2 const& mtv  ) noexcept
{
    auto vel1 = inRegistry.GetComponent<Velocity>( e1 );
    auto vel2 = inRegistry.GetComponent<Velocity>( e2 );
    auto rb1  = inRegistry.GetComponent<Rigidbody>( e1 );
    auto rb2  = inRegistry.GetComponent<Rigidbody>( e2 );

    bool const movable1 = static_cast<bool>(rb1) && static_cast<bool>( vel1 );
    bool const movable2 = static_cast<bool>(rb2) && static_cast<bool>( vel2 );

    if ( !movable1 && !movable2 ) return;

    float share1 = 1.0f;
    float share2 = 1.0f;

    if ( movable1 && movable2 )
    {
        float const m1 = rb1.Value().get().m_Mass;
        float const m2 = rb2.Value().get().m_Mass;
        float const totalMass = m1 + m2;
        share1 = m2 / totalMass; // heavier body yields less
        share2 = m1 / totalMass;
    }
    
    if ( movable1 )
    {
        ApplyCorrection( 
            t1, vel1.Value().get(), { mtv.x() * share1, mtv.y() * share1 } );
    }

    if ( movable2 )
    {
        ApplyCorrection( 
            t2, vel2.Value().get(), { -mtv.x() * share2, -mtv.y() * share2 } );
    }
}
}

void asge::game::systems::MovementSystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [ entity, transform, velocity ] 
            : inRegistry.View<components::Transform, components::Velocity>() )
    {
        transform.get().m_X += velocity.get().m_DX * inDeltaTime;
        transform.get().m_Y += velocity.get().m_DY * inDeltaTime;
    }
}

std::vector<asge::game::systems::CollisionContact> 
asge::game::systems::DetectCollisions(ecs::Registry &inRegistry) noexcept
{
    std::vector<CollisionContact> collisions{};

    auto view = inRegistry.View<components::Transform, components::Collider>();
    for ( auto it = view.begin(); it != view.end(); ++it )
    {
        for ( auto jt = std::next( it ); jt != view.end(); ++jt )
        {
            auto [ e1, t1, c1 ] = (*it);
            auto [ e2, t2, c2 ] = (*jt);

            if ( !details::LayersCanCollide( c1.get(), c2.get() ) ) continue;

            auto const obj1 = WorldBounds( t1.get(), c1.get() );
            auto const obj2 = WorldBounds( t2.get(), c2.get() );

            auto mtv = std::visit( []( auto const& shape1, auto const& shape2 )
            {
                return math::PenetrationVector( shape1, shape2 );
            }, obj1, obj2);

            if ( !mtv ) continue;

            // Unknown means "unrecognized/not configured" (only reachable
            // via a Collider set up outside the normal Solid/Trigger API,
            // e.g. hand-edited TOML) -- ignore the pair entirely rather
            // than guessing whether it should push or trigger.
            bool const c1Unknown = c1.get().m_Resolution == ResolutionType::Unknown;
            bool const c2Unknown = c2.get().m_Resolution == ResolutionType::Unknown;
            if ( c1Unknown || c2Unknown ) continue;

            bool const c1Trigger = c1.get().m_Resolution == ResolutionType::Trigger;
            bool const c2Trigger = c2.get().m_Resolution == ResolutionType::Trigger;

            // A Trigger is always reported as the contact's first entity,
            // regardless of View's own pair order, so a consumer with
            // exactly one Trigger side (the common case) can always treat
            // m_Entity1 as "the trigger" without checking both.
            if ( c2Trigger && !c1Trigger )
                collisions.emplace_back( e2, e1, math::Float2{ -mtv->x(), -mtv->y() }, true );
            else
                collisions.emplace_back( e1, e2, *mtv, c1Trigger || c2Trigger );
        }
    }

    return collisions;
}

void asge::game::systems::ResolveCollisions(
    ecs::Registry &inRegistry, std::span<CollisionContact const> inContacts) noexcept
{
    for ( auto& contact : inContacts )
    {
        if ( contact.m_IsTrigger ) continue;

        auto const e1 = contact.m_Entity1;
        auto const e2 = contact.m_Entity2;

        auto& t1 = inRegistry.GetComponent<components::Transform>( e1 ).Value().get();
        auto& t2 = inRegistry.GetComponent<components::Transform>( e2 ).Value().get();

        auto const mtv = contact.m_Penetration;

        ResolveSolidCollision( inRegistry, e1, t1, e2, t2, mtv );
    }
}

void asge::game::systems::DispatchTriggerEvents(
    PhysicsState &inState, std::span<CollisionContact const> inContacts) noexcept
{
    std::set<ecs::EntityPair> currentTriggerPairs;

    for ( auto& contact : inContacts )
    {
        if ( !contact.m_IsTrigger ) continue;

        auto key = MakeCanonicalPair( contact.m_Entity1, contact.m_Entity2 );
        currentTriggerPairs.insert( key );

        if ( !inState.m_PreviousTriggerPairs.contains( key ) )
            events::OnCollisionTriggerEnter().Emit( contact.m_Entity1, contact.m_Entity2 );
        else
            events::OnCollisionTriggerStay().Emit( contact.m_Entity1, contact.m_Entity2 );
    }

    // Anything overlapping last frame but absent from this frame's contact
    // list — either it stopped overlapping, or an entity was destroyed.
    for ( auto const& pair : inState.m_PreviousTriggerPairs )
    {
        if ( !currentTriggerPairs.contains( pair ) )
            events::OnCollisionTriggerExit().Emit( pair.m_First, pair.m_Second );
    }

    inState.m_PreviousTriggerPairs = std::move( currentTriggerPairs );
}

void asge::game::systems::GravitySystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [e, v, r] : 
            inRegistry.View<components::Velocity, components::Rigidbody>() )
    {
        if ( !r.get().m_AffectedByGravity ) continue;
        v.get().m_DY += kGravity * inDeltaTime;
    }
}

void asge::game::systems::PhysicsUpdate(
    ecs::Registry &inRegistry, PhysicsState &inState, float inDeltaTime) noexcept
{
    GravitySystem( inRegistry, inDeltaTime );
    MovementSystem( inRegistry, inDeltaTime );
    auto contacts = DetectCollisions( inRegistry );
    ResolveCollisions( inRegistry, contacts );
    DispatchTriggerEvents( inState, contacts );
}

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
 *        Solid/Solid overlap. See CollisionResolution for the movable
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

void asge::game::systems::CollisionResolution(ecs::Registry &inRegistry) noexcept
{
    auto view = inRegistry.View<components::Transform, components::Collider>();
    
    for ( auto it = view.begin(); it != view.end(); ++it )
    {
        for ( auto jt = std::next( it ); jt != view.end(); ++jt )
        {
            auto [ e1, t1, c1 ] = (*it);
            auto [ e2, t2, c2 ] = (*jt);

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

            if ( c1Trigger || c2Trigger )
            {
                // Report the Trigger entity first when only one side is one
                // -- "my trigger was entered by X" -- rather than leaving
                // it to View's arbitrary pair-iteration order. When both
                // sides are Triggers there's no meaningful "the" trigger,
                // so (e1, e2) as encountered is as good as any order.
                if ( c2Trigger && !c1Trigger ) events::OnTriggerOverlap().Emit( e2, e1 );
                else                           events::OnTriggerOverlap().Emit( e1, e2 );
                continue;
            }

            ResolveSolidCollision( inRegistry, e1, t1.get(), e2, t2.get(), *mtv );
        }
    }
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

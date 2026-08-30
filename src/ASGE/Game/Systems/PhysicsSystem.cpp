#include "PhysicsSystem.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Velocity.hpp"
#include "../Components/Collider.hpp"
#include "../Components/Rigidbody.hpp"

#include <ASGE/Core/Math/Geometry/Collision.hpp>

namespace
{
using namespace asge::game::components;

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

            auto vel1 = inRegistry.GetComponent<components::Velocity>( e1 );
            auto vel2 = inRegistry.GetComponent<components::Velocity>( e2 );
            auto rb1  = inRegistry.GetComponent<components::Rigidbody>( e1 );
            auto rb2  = inRegistry.GetComponent<components::Rigidbody>( e2 );

            bool const movable1 = static_cast<bool>(rb1) && static_cast<bool>( vel1 );
            bool const movable2 = static_cast<bool>(rb2) && static_cast<bool>( vel2 );

            if ( !movable1 && !movable2 ) continue;

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
                ApplyCorrection( t1.get(), vel1.Value().get(),
                    { mtv->x() * share1, mtv->y() * share1 } );
            }

            if ( movable2 )
            {
                ApplyCorrection( t2.get(), vel2.Value().get(),
                    { -mtv->x() * share2, -mtv->y() * share2 } );
            }
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

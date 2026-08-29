#include "PhysicsSystem.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Velocity.hpp"
#include "../Components/Collider.hpp"

#include <ASGE/Core/Math/Geometry/Rect.hpp>

namespace
{
using namespace asge::game::components;

asge::math::Rect WorldBounds( Transform const& inT, Collider const& inC ) noexcept
{
    return {
        inT.m_X + inC.m_LocalBounds.x,
        inT.m_Y + inC.m_LocalBounds.y,
        inC.m_LocalBounds.w,
        inC.m_LocalBounds.h
    };
}

// Applies a positional correction and zeroes velocity on whichever axis moved.
void ApplyCorrection( Transform& inT, Velocity& inV, asge::math::Float2 const& inDelta ) noexcept
{
    inT.m_X += inDelta.x();
    inT.m_Y += inDelta.y();
    if ( inDelta.x() != 0.0f ) inV.m_DX = 0.0f;
    if ( inDelta.y() != 0.0f ) inV.m_DY = 0.0f;
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

            auto mtv = math::PenetrationVector( obj1, obj2 );
            if ( !mtv ) continue;

            auto vel1 = inRegistry.GetComponent<components::Velocity>( e1 );
            auto vel2 = inRegistry.GetComponent<components::Velocity>( e2 );
            bool const movable1 = static_cast<bool>( vel1 );
            bool const movable2 = static_cast<bool>( vel2 );

            if ( !movable1 && !movable2 ) continue;

            float const share1 = ( movable1 && movable2 ) ? 0.5f : ( movable1 ? 1.0f : 0.0f );
            float const share2 = ( movable1 && movable2 ) ? 0.5f : ( movable2 ? 1.0f : 0.0f );

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
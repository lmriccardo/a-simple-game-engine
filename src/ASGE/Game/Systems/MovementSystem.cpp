#include "MovementSystem.hpp"

void asge::game::systems::MovementSystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [ entity, transform, velocity ] 
            : inRegistry.View<components::Transform, components::Velocity>() )
    {
        transform.get().m_X += velocity.get().m_DX * inDeltaTime;
        transform.get().m_Y += velocity.get().m_DY * inDeltaTime;
    }
}
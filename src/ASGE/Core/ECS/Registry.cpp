#include "Registry.hpp"

asge::Result<asge::ecs::Entity> asge::ecs::Registry::CreateEntity() noexcept
{
    return m_Allocator.Create();
}

std::vector<asge::ecs::Entity> asge::ecs::Registry::AllEntities() const noexcept
{
    return m_Allocator.AllEntities();
}

asge::BoolResult asge::ecs::Registry::DestroyEntity(Entity inEntity) noexcept
{
    if ( auto result = m_Allocator.Destroy( inEntity ); !result )
    {
        return result;
    }

    // Cross-cutting concern: strip this entity from every pool,
    // regardless of type. Each pool no-ops safely if it doesn't
    // contain inEntity — see ComponentPool::Remove.
    for ( auto& pool : m_Pools )
    {
        if ( pool )
        {
            (void)pool->Remove( inEntity );
        }
    }

    return BoolResult::Ok();
}

void asge::ecs::Registry::DestroyAllEntities() noexcept
{
    for ( auto entity : AllEntities() )
    {
        if ( auto result = DestroyEntity( entity ); !result )
        {
            result.LogError();
        }
    }
}

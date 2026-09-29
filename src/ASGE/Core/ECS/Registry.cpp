#include "Registry.hpp"
#include "Hierarchy.hpp"

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
    // Unlink from the Hierarchy first: left in place, the parent's
    // m_FirstChild/m_LastChild and the siblings' links would keep pointing at
    // this entity, and its own children at a dead parent.
    if ( HasComponent<components::Hierarchy>( inEntity ) )
    {
        std::vector<Entity> children;
        components::ForEachChild( *this, inEntity, [&]( Entity inChild ) { children.push_back( inChild ); } );
        for ( auto child : children ) components::DetachChild( *this, child );
        components::DetachChild( *this, inEntity );
    }

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

void asge::ecs::Registry::DisableEntity(Entity inEntity) noexcept
{
    AddComponent( inEntity, markers::Disable{} );
}

bool asge::ecs::Registry::IsDisabled(Entity inEntity) const noexcept
{
    Entity current = inEntity;
    while ( current != Entity::Null() )
    {
        if ( HasComponent<markers::Disable>( current ) ) return true;

        auto hResult = GetComponent<components::Hierarchy>( current );
        if ( !hResult ) return false;
        current = hResult.Value().get().m_Parent;
    }

    return false;
}

void asge::ecs::Registry::ForEachEntity(std::function<void(Entity const &)> m_Callback) const
{
    for ( auto const& entity : AllEntities() )
    {
        m_Callback( entity );
    }
}

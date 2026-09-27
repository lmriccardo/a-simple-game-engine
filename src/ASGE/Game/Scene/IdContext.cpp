#include "IdContext.hpp"

int asge::game::scene::SaveContext::Resolve(ecs::Entity inEntity) const noexcept
{
    if ( inEntity == ecs::Entity::Null() ) return -1;
    auto it = m_Ids.find( inEntity );
    return it != m_Ids.end() ? static_cast<int>( it->second ) : -1;
}

asge::ecs::Entity asge::game::scene::LoadContext::Resolve(int inSaveId) const noexcept
{
    if ( inSaveId < 0 ) return ecs::Entity::Null();
    auto it = m_Entities.find(static_cast<uint32_t>(inSaveId));
    return it != m_Entities.end() ? it->second : ecs::Entity::Null();
}

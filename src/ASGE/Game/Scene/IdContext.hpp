#pragma once

#include <unordered_map>
#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::scene
{

/**
 * @brief Maps live entities to the stable ids written in a scene file.
 *
 * Handed to every Serializer::ToToml so an entity reference (like a
 * Hierarchy parent) can be saved as an id instead of a raw handle, which
 * would not survive a reload.
 */
struct SaveContext
{
    std::unordered_map<ecs::Entity, std::uint32_t> m_Ids; // Save id of every entity being written

    /**
     * @brief Returns the save id of inEntity.
     *
     * Gives -1 for Entity::Null() and for an entity that is not in m_Ids.
     */
    [[nodiscard]] int Resolve( ecs::Entity inEntity ) const noexcept;
};

/**
 * @brief Maps the stable ids in a scene file back to the entities created for them.
 *
 * Handed to every Serializer::FromToml; the inverse of SaveContext.
 */
struct LoadContext
{
    std::unordered_map<std::uint32_t, ecs::Entity> m_Entities; // New entity created for each save id

    /**
     * @brief Returns the entity created for inSaveId.
     *
     * Gives Entity::Null() for a negative id and for an id with no entity.
     */
    [[nodiscard]] ecs::Entity Resolve( int inSaveId ) const noexcept;
};

}
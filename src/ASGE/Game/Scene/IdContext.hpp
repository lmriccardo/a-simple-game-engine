#pragma once

#include <unordered_map>
#include <ASGE/Core/ECS/Entity.hpp>

namespace asge::game::scene
{

struct SaveContext
{
    std::unordered_map<ecs::Entity, std::uint32_t> m_Ids;
    [[nodiscard]] int Resolve( ecs::Entity inEntity ) const noexcept;
};

struct LoadContext
{
    std::unordered_map<std::uint32_t, ecs::Entity> m_Entities;
    [[nodiscard]] ecs::Entity Resolve( int inSaveId ) const noexcept;
};

}
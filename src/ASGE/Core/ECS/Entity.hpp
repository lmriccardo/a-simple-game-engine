#pragma once

#include <cstdint>
#include <limits>
#include <array>

namespace asge::ecs
{

using EntityIndex      = std::uint32_t;
using EntityGeneration = std::uint32_t;

/**
 * @brief A lightweight, generational handle identifying an entity in the ECS
 *
 * An Entity carries no data or behaviour of its own; it is just a key used to
 * look up components in the sparse/dense arrays that back the ECS storage.
 * Pairing the slot index with a generation counter lets a stale handle be
 * detected and rejected once its slot has been recycled for a new entity,
 * rather than silently aliasing onto unrelated data.
 */
struct Entity
{
    EntityIndex      m_Index;        // Where it sits in the sparse arrays
    EntityGeneration m_Generation;   // How many times that slot has been reused

    friend bool operator==(Entity const&, Entity const&) = default;

    /* Returns a Null Entity, i.e., with index value set at its maximum */
    inline static constexpr Entity Null() noexcept
    {
        return Entity{ std::numeric_limits<EntityIndex>::max(), 0 };
    }
};

template<std::size_t N>
class EntityAllocator
{
private:
    
public:
};

}
#pragma once

#include <cstdint>
#include <limits>
#include <utility>

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

/** @brief Packs an Entity's index+generation into one orderable integer — the basis for EntityPair's operator<. */
inline std::uint64_t PackEntity( Entity inE ) noexcept
{
    return ( static_cast<std::uint64_t>( inE.m_Index ) << 32 ) | inE.m_Generation;
}

/**
 * @brief An unordered pair of Entity, usable as a std::set/map key.
 *
 * Two EntityPair values compare equal (neither is less than the other)
 * regardless of which entity was m_First/m_Second when constructed — see
 * MakeCanonicalPair, which is how one gets built consistently.
 */
struct EntityPair
{
    Entity m_First;
    Entity m_Second;

    friend inline bool operator<(
        EntityPair const& inL, EntityPair const& inR ) noexcept
    {
        auto const p1 = std::pair{ PackEntity(inL.m_First), PackEntity(inL.m_Second) };
        auto const p2 = std::pair{ PackEntity(inR.m_First), PackEntity(inR.m_Second) };
        return p1 < p2;
    }
};

/** @brief Builds an EntityPair with the lower-packed Entity always first, so (a, b) and (b, a) produce the same key. */
inline EntityPair MakeCanonicalPair( Entity inA, Entity inB ) noexcept
{
    return PackEntity(inA) < PackEntity(inB)
        ? EntityPair{ inA, inB } : EntityPair{ inB, inA };
}

}
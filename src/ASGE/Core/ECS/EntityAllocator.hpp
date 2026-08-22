#pragma once

#include <array>
#include <string>
#include <ASGE/Core/Memory/FreeList.hpp>
#include <ASGE/Core/Errors.hpp>
#include "Entity.hpp"

namespace asge::ecs
{

/**
 * @brief Fixed-capacity issuer and validator of generational @c Entity handles.
 *
 * Wraps a @c mem::FreeList<N> of slot indices with a per-slot generation
 * counter. @c Create() claims a free index and pairs it with that slot's
 * current generation; @c Destroy() returns the index to the free-list and
 * then bumps the slot's generation, which invalidates every @c Entity
 * handle previously issued for that slot. @c IsAlive() is the source of
 * truth for handle validity: a handle is alive only while its slot is
 * currently claimed and its generation matches the slot's current one —
 * so a handle built for a never-allocated slot is correctly rejected too.
 *
 * A slot whose generation reaches its maximum value is retired rather than
 * recycled, so a still-held stale handle can never alias a future
 * allocation once the counter would otherwise wrap back to a reused value.
 *
 * @tparam N Number of concurrently allocatable entities; fixed at compile time.
 */
template<std::size_t N>
class EntityAllocator
{
private:
    mem::FreeList<N>                m_EntityFreeIds;
    std::array<EntityGeneration, N> m_Generations{};
public:
    EntityAllocator()
    {
        // Initially fills all generations to 0
        m_Generations.fill(0);
    }

    EntityAllocator(EntityAllocator const&) = delete;
    EntityAllocator(EntityAllocator&&) = default;
    EntityAllocator& operator=(EntityAllocator const&) = delete;
    EntityAllocator& operator=(EntityAllocator&&) = default;

    /**
     * @brief Claims a free slot and returns a live handle for it.
     *
     * The returned Entity pairs the slot's index with its current generation.
     * @return A fresh Entity, or an error if all N slots are in use.
     */
    [[nodiscard]] Result<Entity> Create() noexcept
    {
        auto getResult = m_EntityFreeIds.Get();
        if ( !getResult ) return Result<Entity>::Err(make_error_code( 
            errors::EcsError::NoMoreEntityAvailable ));
        
        auto index = static_cast<EntityIndex>(getResult.Value());
        return Result<Entity>::Ok( Entity{ index, m_Generations[index] } );
    }

    /**
     * @brief Invalidates a live handle and returns its slot to the free-list.
     *
     * Bumps the slot's generation, so every Entity previously issued for
     * it (including inEntity) subsequently fails IsAlive().
     * @return Ok on success, or an error if inEntity is not currently alive.
     */
    [[nodiscard]] BoolResult Destroy( Entity inEntity ) noexcept
    {
        if ( !IsAlive( inEntity ) )
        {
            return BoolResult::Err(
                make_error_code( errors::EcsError::EntityIsNotAlive ),
                "Id " + std::to_string(inEntity.m_Index)
            );
        }

        EntityGeneration& generation = m_Generations[inEntity.m_Index];
        EntityGeneration const nextGeneration = generation + 1;
        bool const isRetiring = nextGeneration == std::numeric_limits<EntityGeneration>::max();

        // Once the next generation would hit the reserved maximum, retire the
        // slot instead of recycling it (see class docs): skip returning it to
        // the free-list so it can never be handed out — and thus aliased — again.
        if ( !isRetiring )
        {
            auto freeResult = m_EntityFreeIds.Free( inEntity.m_Index );
            if ( !freeResult ) return freeResult;
        }

        generation = nextGeneration;
        return BoolResult::Ok();
    }

    /**
     * @brief Checks whether a handle still refers to the slot it was issued for.
     *
     * True only when the slot is currently claimed (i.e. was returned by
     * Create() and not since destroyed) and its generation matches.
     */
    [[nodiscard]] bool IsAlive( Entity inEntity ) const noexcept
    {
        return m_EntityFreeIds.IsUsed( inEntity.m_Index )
            && inEntity.m_Generation == m_Generations[inEntity.m_Index];
    }
};

}
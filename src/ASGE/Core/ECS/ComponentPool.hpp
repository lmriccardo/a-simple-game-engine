#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <type_traits>
#include <system_error>
#include <utility>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/TypeInfo.hpp>
#include <span>
#include "Entity.hpp"
#include "Constant.hpp"

namespace asge::ecs
{

class IComponentPool
{
public:
    [[nodiscard]] virtual BoolResult Remove( Entity ) noexcept = 0;
    [[nodiscard]] virtual bool Contains( Entity ) const noexcept = 0;
    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;
    [[nodiscard]] virtual std::span<Entity const> Entities() const noexcept = 0;
};

/**
 * @brief Fixed-capacity sparse-set storage of one component type per Entity.
 *
 * Maps each entity index to a densely-packed component array, so Insert,
 * Get, Remove and Contains are all O(1) and iteration only ever touches
 * live components. Removal is swap-and-pop, so iteration order can change
 * after a Remove() call.
 *
 * @warning Components are tracked by entity index alone, not generation:
 *          this pool is never notified when an entity is destroyed, so a
 *          recycled index will alias its previous occupant's component
 *          unless the caller removes it first.
 *
 * @tparam T  Component type; must be default-constructible and move-assignable.
 * @tparam Ne Number of distinct entity indices this pool can address.
 * @tparam Nc Maximum number of components concurrently stored.
 */
template<typename T, std::size_t Nc = asge::ecs::kMaxEntities, std::size_t Ne = asge::ecs::kMaxEntities>
requires std::is_default_constructible_v<std::decay_t<T>> && std::is_move_assignable_v<std::decay_t<T>>
class ComponentPool final : public IComponentPool
{
    std::array<std::size_t, Ne> m_Sparse{};         // Maps to each entity an index in the dense array
    std::array<Entity, Nc>      m_DenseEntity{};    // An array of entity that have a component in the pool
    std::array<T, Nc>           m_DenseComponent{}; // All components associated with entities

    std::size_t m_FreeComponentHead{0};

    // Forward iterator over (Entity const&, T&) pairs, in dense storage order.
    class _Iterator
    {
        ComponentPool* m_Pool;  // The actual pool on which iterate on
        std::size_t    m_Index; // The current index of the iterator
    public:
        _Iterator( ComponentPool* inPool, std::size_t inStartIndex )
            : m_Pool( inPool ), m_Index( inStartIndex ) {}
        
        auto operator*() const
        {
            return std::pair<Entity const&, T&>(
                m_Pool->m_DenseEntity[m_Index], m_Pool->m_DenseComponent[m_Index]
            );
        }

        _Iterator& operator++() { ++m_Index; return *this; }
        bool operator!=(_Iterator const& other) const { return m_Index != other.m_Index; }
    };

public:
    using iterator = _Iterator;

    // std::variant (which Result<T> is built on) cannot hold a reference
    // type, so a "reference to the stored component" is expressed as a
    // std::reference_wrapper — implicitly convertible back to T&.
    using component_ref = std::reference_wrapper<T>;

    ComponentPool()
    {
        // Fills the sparse array with an invalid index for 
        // both dense entity and dense component array
        m_Sparse.fill( Nc );
    }

    ComponentPool(ComponentPool const&) = delete;
    ComponentPool(ComponentPool&&) = default;
    ComponentPool& operator=(ComponentPool const&) = delete;
    ComponentPool& operator=(ComponentPool&&) = default;

    /**
     * @brief Attaches inEntity's component, replacing it if one already exists.
     *
     * Reuses the entity's existing slot if it already has a component here,
     * otherwise claims a fresh one.
     * @return A reference to the stored component, or an error if inEntity's
     *         index is out of range or the pool is full.
     */
    [[nodiscard]] Result<component_ref> Insert( Entity inEntity, T inComponent )
    {
        // Simple safe guard against out of range entity indexes (maybe
        // hand-produced by someone rather than EntityAllocator).
        if ( inEntity.m_Index >= Ne )
        {
            return Result<component_ref>::Err(std::make_error_code( std::errc::result_out_of_range ),
                "entity Id " + std::to_string(inEntity.m_Index) +
                " >= " + std::to_string(Ne)
            );
        }

        std::size_t insertPosition;

        if ( m_Sparse[inEntity.m_Index] < Nc )
        {
            // Entity already has this component — reuse its existing slot.
            // No capacity check needed, no bump of m_FreeComponentHead.
            insertPosition = m_Sparse[inEntity.m_Index];
        }
        else
        {
            // Brand new insertion — needs a fresh slot, so capacity matters.
            if ( m_FreeComponentHead >= Nc )
            {
                return Result<component_ref>::Err( make_error_code(errors::EcsError::NoMoreComponentsAvailable) );
            }

            insertPosition = m_FreeComponentHead++;
        }

        m_Sparse[inEntity.m_Index] = insertPosition;
        m_DenseEntity[insertPosition] = inEntity;
        m_DenseComponent[insertPosition] = std::move(inComponent);

        return Result<component_ref>::Ok( component_ref( m_DenseComponent[insertPosition] ) );
    }

    /** @brief Checks whether inEntity currently has a component in this pool. */
    [[nodiscard]] bool Contains( Entity inEntity ) const noexcept override
    {
        return inEntity.m_Index < m_Sparse.size() 
            && m_Sparse[inEntity.m_Index] < m_DenseEntity.size()
            && m_DenseEntity[m_Sparse[inEntity.m_Index]] == inEntity;
    }

    /**
     * @brief Looks up inEntity's component.
     * @return A reference to the component, or an error if inEntity has none.
     */
    [[nodiscard]] Result<component_ref> Get( Entity inEntity ) noexcept
    {
        // At first check if the input entity is conatined into the component pool
        if ( !Contains( inEntity ) )
        {
            auto const ec = make_error_code( errors::EcsError::EntityNotAttachedToComponent);
            return Result<component_ref>::Err( ec, "name " + rtti::GetDemangledName<T>());
        }

        return Result<component_ref>::Ok( component_ref( m_DenseComponent[m_Sparse[inEntity.m_Index]] ) );
    }

    /**
     * @brief Detaches inEntity's component, if any.
     *
     * Swaps the removed slot with the last dense component to keep storage
     * packed, so iteration order may change after this call.
     * @return Ok on success, or an error if inEntity has no component here.
     */
    [[nodiscard]] BoolResult Remove( Entity inEntity ) noexcept override
    {
        // At first check if the input entity is conatined into the component pool
        if ( !Contains( inEntity ) )
        {
            auto const ec = make_error_code( errors::EcsError::EntityNotAttachedToComponent);
            return BoolResult::Err( ec, "name " + rtti::GetDemangledName<T>());
        }

        auto densePosition = m_Sparse[inEntity.m_Index];
        auto lastPlacedComponent = --m_FreeComponentHead;
        
        if (densePosition != lastPlacedComponent)
        {
            m_DenseEntity[densePosition] = std::move(m_DenseEntity[lastPlacedComponent]);
            m_DenseComponent[densePosition] = std::move(m_DenseComponent[lastPlacedComponent]);
            m_Sparse[m_DenseEntity[densePosition].m_Index] = densePosition;
        }

        m_Sparse[inEntity.m_Index] = Nc;

        return BoolResult::Ok();
    }

    /** @brief Returns the total number of components associated with entities */
    [[nodiscard]] std::size_t Size() const noexcept override
    {
        return m_FreeComponentHead;
    }

    /** @brief Returns a view over all entities in the pool */
    std::span<Entity const> Entities() const noexcept override
    {
        return std::span<Entity const>( m_DenseEntity.data(), Size() );
    }

    /** @brief Iterates all (Entity, T&) pairs currently stored, in dense order. */
    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, m_FreeComponentHead); }
};

}
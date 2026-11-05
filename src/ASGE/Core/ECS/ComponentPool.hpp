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

/**
 * @brief Type-erased interface of every ComponentPool, so Registry can hold them in one list.
 *
 * Exposes only what does not depend on the component type: removing an
 * entity's component, membership, size and the entity list.
 */
class IComponentPool
{
public:
    /**
     * @brief Virtual so a pool can be destroyed through the interface.
     *
     * Registry deletes concrete pools via `std::unique_ptr<IComponentPool>`;
     * without this that delete would skip T's destructors (undefined behavior).
     */
    virtual ~IComponentPool() = default;

    /**
     * @brief Detaches the entity's component, if it has one.
     *
     * Returns an error if the entity has no component in this pool.
     */
    [[nodiscard]] virtual BoolResult Remove( Entity ) noexcept = 0;

    /**
     * @brief True if the entity currently has a component here.
     */
    [[nodiscard]] virtual bool Contains( Entity ) const noexcept = 0;

    /**
     * @brief Number of components currently stored.
     */
    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;

    /**
     * @brief The entities that have a component, in dense storage order.
     */
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
 * Components are tracked by entity index alone, not generation: this pool is
 * never notified when an entity is destroyed, so a recycled index will alias
 * its previous occupant's component unless the caller removes it first.
 *
 * T must be default-constructible and move-assignable, Ne is the number of
 * distinct entity indices the pool can address and Nc the maximum number of
 * components stored at once.
 */
template<typename T, std::size_t Nc = asge::ecs::kMaxEntities, std::size_t Ne = asge::ecs::kMaxEntities>
requires std::is_default_constructible_v<std::decay_t<T>> && std::is_move_assignable_v<std::decay_t<T>>
class ComponentPool final : public IComponentPool
{
    std::array<std::size_t, Ne> m_Sparse{};         // Maps to each entity an index in the dense array
    std::array<Entity, Nc>      m_DenseEntity{};    // An array of entity that have a component in the pool
    std::array<T, Nc>           m_DenseComponent{}; // All components associated with entities

    std::size_t m_FreeComponentHead{0}; // Next free dense slot, also the component count

    /**
     * @brief Forward iterator over (Entity const&, T&) pairs.
     *
     * Walks the dense storage in order, so it visits live components only.
     */
    class _Iterator
    {
        ComponentPool* m_Pool;  // The actual pool on which iterate on
        std::size_t    m_Index; // The current index of the iterator
    public:
        /**
         * @brief Points the iterator at dense position inStartIndex of inPool.
         */
        _Iterator( ComponentPool* inPool, std::size_t inStartIndex )
            : m_Pool( inPool ), m_Index( inStartIndex ) {}

        /**
         * @brief The entity and a reference to its component at this position.
         */
        auto operator*() const
        {
            return std::pair<Entity const&, T&>(
                m_Pool->m_DenseEntity[m_Index], m_Pool->m_DenseComponent[m_Index]
            );
        }

        /**
         * @brief Steps to the next dense position.
         */
        _Iterator& operator++() { ++m_Index; return *this; }

        /**
         * @brief True if the two iterators are at different positions.
         */
        bool operator!=(_Iterator const& other) const { return m_Index != other.m_Index; }
    };

public:
    using iterator = _Iterator;

    // std::variant (which Result<T> is built on) cannot hold a reference
    // type, so a "reference to the stored component" is expressed as a
    // std::reference_wrapper — implicitly convertible back to T&.
    using component_ref = std::reference_wrapper<T>;
    using const_component_ref = std::reference_wrapper<T const>; // Read-only counterpart, for the const Get() overload

    /**
     * @brief Creates an empty pool with every sparse entry marked invalid.
     */
    ComponentPool()
    {
        // Fills the sparse array with an invalid index for 
        // both dense entity and dense component array
        m_Sparse.fill( Nc );
    }

    /**
     * @brief Copy construction is disabled.
     */
    ComponentPool(ComponentPool const&) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    ComponentPool(ComponentPool&&) = default;

    /**
     * @brief Copy assignment is disabled.
     */
    ComponentPool& operator=(ComponentPool const&) = delete;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    ComponentPool& operator=(ComponentPool&&) = default;

    /**
     * @brief Attaches inEntity's component, replacing it if one already exists.
     *
     * Reuses the entity's existing slot if it already has a component here,
     * otherwise claims a fresh one. Returns a reference to the stored
     * component, or an error if inEntity's index is out of range or the pool
     * is full. The reference is invalidated by a later Remove() that moves it.
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

    /**
     * @brief Checks whether inEntity currently has a component in this pool.
     *
     * Compares the full handle, generation included, so a stale handle to a
     * recycled index reports false.
     */
    [[nodiscard]] bool Contains( Entity inEntity ) const noexcept override
    {
        return inEntity.m_Index < m_Sparse.size() 
            && m_Sparse[inEntity.m_Index] < m_DenseEntity.size()
            && m_DenseEntity[m_Sparse[inEntity.m_Index]] == inEntity;
    }

    /**
     * @brief Looks up inEntity's component (read-only).
     *
     * Returns a const reference to the component, or an error
     * (EcsError::EntityNotAttachedToComponent) if inEntity has none.
     */
    [[nodiscard]] Result<const_component_ref> Get( Entity inEntity ) const noexcept
    {
        // At first check if the input entity is conatined into the component pool
        if ( !Contains( inEntity ) )
        {
            auto const ec = make_error_code( errors::EcsError::EntityNotAttachedToComponent);
            return Result<const_component_ref>::Err( ec, "name " + rtti::GetDemangledName<T>());
        }

        return Result<const_component_ref>::Ok( const_component_ref( m_DenseComponent[m_Sparse[inEntity.m_Index]] ) );
    }

    /**
     * @brief Looks up inEntity's component.
     *
     * Implemented in terms of the const overload above (then const_cast-ing
     * the result back to mutable) rather than duplicating the lookup logic —
     * safe here because *this is genuinely non-const at the call site.
     * Returns a reference to the component, or an error if inEntity has none.
     */
    [[nodiscard]] Result<component_ref> Get( Entity inEntity ) noexcept
    {
        auto result = std::as_const(*this).Get( inEntity );
        if ( !result ) return Result<component_ref>::Err( result.Error() );
        return Result<component_ref>::Ok( component_ref( const_cast<T&>( result.Value().get() ) ) );
    }

    /**
     * @brief Detaches inEntity's component, if any.
     *
     * Swaps the removed slot with the last dense component to keep storage
     * packed, so iteration order may change after this call. Returns an error
     * if inEntity has no component here.
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

    /**
     * @brief Returns the total number of components associated with entities.
     */
    [[nodiscard]] std::size_t Size() const noexcept override
    {
        return m_FreeComponentHead;
    }

    /**
     * @brief Returns a view over all entities in the pool.
     *
     * Valid until the next Insert or Remove changes the storage.
     */
    std::span<Entity const> Entities() const noexcept override
    {
        return std::span<Entity const>( m_DenseEntity.data(), Size() );
    }

    /**
     * @brief Start of the (Entity, T&) pairs, in dense order.
     *
     * Do not Insert or Remove while iterating.
     */
    iterator begin() { return iterator(this, 0); }

    /**
     * @brief One past the last stored pair.
     */
    iterator end() { return iterator(this, m_FreeComponentHead); }
};

}
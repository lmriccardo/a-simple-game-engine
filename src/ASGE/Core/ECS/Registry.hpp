#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <ASGE/Core/Errors.hpp>
#include "EntityAllocator.hpp"
#include "ComponentPool.hpp"
#include "Constant.hpp"
#include "ComponentTypeId.hpp"
#include "View.hpp"

namespace asge::ecs
{

/**
 * @brief Owns entities and their components; the ECS's single entry point.
 *
 * Wraps an EntityAllocator<kMaxEntities> for entity lifecycle and one
 * ComponentPool<T> per component type used so far (created lazily on the
 * first AddComponent<T>) and ties them together: DestroyEntity() strips
 * the entity from every pool, and View<Ts...>() iterates entities that
 * have every one of Ts across their pools.
 */
class Registry
{
    EntityAllocator<kMaxEntities>                m_Allocator{};
    std::vector<std::unique_ptr<IComponentPool>> m_Pools{};

    // Concrete pool type backing component type T.
    template<typename T>
    using pool_t = ComponentPool<T, component_cap_v<T>>;

    // Returns T's pool, or nullptr if T has never been used (read-only).
    template <typename T>
    [[nodiscard]] pool_t<T> const* FindPool() const noexcept
    {
        ComponentTypeId id = GetComponentTypeId<T>();
        if ( id >= m_Pools.size() || !m_Pools[id] )
        {
            return nullptr;
        }
        return static_cast<pool_t<T> const*>( m_Pools[id].get() );
    }

    // Returns T's pool, or nullptr if T has never been used. Implemented in
    // terms of the const overload above (then const_cast-ing the pointer
    // back to mutable) rather than duplicating the lookup — safe here
    // because *this is genuinely non-const at the call site.
    template <typename T>
    [[nodiscard]] pool_t<T>* FindPool() noexcept
    {
        return const_cast<pool_t<T>*>( std::as_const(*this).template FindPool<T>() );
    }

    // Returns T's pool, creating (and registering) it on first use.
    template <typename T>
    [[nodiscard]] pool_t<T>& GetOrCreatePool()
    {
        if ( pool_t<T>* existing = FindPool<T>() ) return *existing;
        ComponentTypeId id = GetComponentTypeId<T>();
        if ( id >= m_Pools.size() )
        {
            m_Pools.resize( id + 1 );
        }
        m_Pools[id] = std::make_unique<pool_t<T>>();
        return static_cast<pool_t<T>&>( *m_Pools[id] );
    }

public:
    /**
     * @brief Allocates a new entity.
     * @return A fresh Entity, or an error if the registry has no more room.
     */
    [[nodiscard]] Result<Entity> CreateEntity() noexcept;

    /**
     * @brief Lists every currently-alive entity, regardless of which
     *        components (if any) it has.
     *
     * Unlike View<Ts...>, which only sees entities present in a specific
     * set of component pools, this walks the allocator directly — the
     * only way to reach an entity with no components yet, or to visit
     * every entity for something like scene serialization.
     */
    [[nodiscard]] std::vector<Entity> AllEntities() const noexcept;

    /**
     * @brief Destroys an entity and strips it from every component pool.
     * @return Ok on success, or an error if inEntity is not currently alive.
     */
    [[nodiscard]] BoolResult DestroyEntity( Entity inEntity ) noexcept;

    /**
     * @brief Destroys an entities and strips them from every component pool.
     *        Logs error when it cannot remove a specific entity.
     */
    [[nodiscard]] void DestroyAllEntities() noexcept;

    /**
     * @brief Attaches inComponent to inEntity, creating T's pool on first use.
     * @return A reference to the stored component, or an error if inEntity's
     *         index is out of range or T's pool is full.
     */
    template<typename T>
    Result<std::reference_wrapper<T>> AddComponent( Entity inEntity, T inComponent )
    {
        return GetOrCreatePool<T>().Insert( inEntity, std::move( inComponent ) );
    }

    /**
     * @brief Detaches inEntity's component of type T, if any.
     * @return Ok on success, or an error if T has never been used or
     *         inEntity has no component of this type.
     */
    template<typename T>
    [[nodiscard]] BoolResult RemoveComponent( Entity inEntity )
    {
        pool_t<T>* pool = FindPool<T>();
        if ( !pool )
        {
            return BoolResult::Err( 
                make_error_code( errors::EcsError::InvalidComponent ),
                rtti::GetDemangledName<T>()
            );
        }
        return pool->Remove( inEntity );
    }

    /**
     * @brief Looks up inEntity's component of type T (read-only).
     * @return A const reference to the component, or an error if T has
     *         never been used or inEntity has none.
     */
    template<typename T>
    [[nodiscard]] Result<std::reference_wrapper<T const>> GetComponent( Entity inEntity ) const noexcept
    {
        pool_t<T> const* pool = FindPool<T>();
        if ( !pool )
        {
            return Result<std::reference_wrapper<T const>>::Err(
                make_error_code( errors::EcsError::InvalidComponent ),
                rtti::GetDemangledName<T>()
            );
        }
        return pool->Get(inEntity);
    }

    /**
     * @brief Looks up inEntity's component of type T.
     * @return A reference to the component, or an error if T has never
     *         been used or inEntity has none.
     */
    template<typename T>
    [[nodiscard]] Result<std::reference_wrapper<T>> GetComponent( Entity inEntity ) noexcept
    {
        pool_t<T>* pool = FindPool<T>();
        if ( !pool )
        {
            return Result<std::reference_wrapper<T>>::Err(
                make_error_code( errors::EcsError::InvalidComponent ),
                rtti::GetDemangledName<T>()
            );
        }
        return pool->Get(inEntity);
    }

    /** @brief Checks whether inEntity currently has a component of type T. */
    template <typename T>
    [[nodiscard]] bool HasComponent( Entity inEntity ) const noexcept
    {
        pool_t<T> const* pool = FindPool<T>();
        return pool && pool->Contains( inEntity );
    }

    /**
     * @brief Returns a lazy view over entities that have every component in Ts.
     *
     * Looks up each type's pool via FindPool — no pool is created for a
     * type that has never been used — so the result may be empty if any
     * of Ts has never been added to an entity yet.
     *
     * @tparam Ts Component types the returned view requires.
     * @return A View<Ts...> yielding (Entity, Ts&...) for each match.
     */
    template<typename ... Ts>
    [[nodiscard]] asge::ecs::View<Ts...> View() noexcept
    {
        return asge::ecs::View<Ts...>(FindPool<Ts>()...);
    }
};

}
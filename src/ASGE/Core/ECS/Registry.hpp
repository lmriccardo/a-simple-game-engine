#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <functional>
#include <ASGE/Core/Errors.hpp>
#include "EntityAllocator.hpp"
#include "ComponentPool.hpp"
#include "Constant.hpp"
#include "ComponentTypeId.hpp"
#include "View.hpp"
#include "ResourceHolder.hpp"

namespace asge::ecs
{

/**
 * @brief Owns entities, their components, and registry-wide resources.
 *
 * The ECS's single entry point. Wraps an EntityAllocator<kMaxEntities> for entity lifecycle and one
 * ComponentPool<T> per component type used so far (created lazily on the
 * first AddComponent<T>) and ties them together: DestroyEntity() strips
 * the entity from every pool, and View<Ts...>() iterates entities that
 * have every one of Ts across their pools. `SetResource<T>`/`GetResource<T>`
 * hold at most one T not tied to any Entity — e.g. shared/global state a
 * system needs but that isn't itself part of the world.
 */
class Registry
{
    EntityAllocator<kMaxEntities>                 m_Allocator{};  // Issues and validates entity handles
    std::vector<std::unique_ptr<IComponentPool>>  m_Pools{};      // One pool per component type, indexed by type id
    std::vector<std::unique_ptr<IResourceHolder>> m_Resources{};  // dense, indexed by ResourceId<T>()

    // Next unused resource id, shared by every Registry instance -- same
    // "dense, monotonically-assigned, unsynchronized" tradeoff as
    // ComponentTypeId.hpp's GetComponentTypeId, just kept private to Registry
    // since resource ids (unlike component type ids) have no other caller.
    static inline std::size_t s_NextResourceId = 0; // Counter behind ResourceId<T>()

    // Concrete pool type backing component type T.
    template<typename T>
    using pool_t = ComponentPool<T, component_cap_v<T>>;

    /**
     * @brief Returns the stable id for resource type T.
     *
     * Assigned on first use and cached in a function-local static -- see
     * s_NextResourceId's note above about assignment order and thread-safety.
     */
    template <typename T>
    [[nodiscard]] static std::size_t ResourceId() noexcept
    {
        static std::size_t const id = s_NextResourceId++;
        return id;
    }

    /**
     * @brief Returns T's pool (read-only), or nullptr if T has never been used.
     */
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

    /**
     * @brief Returns T's pool, or nullptr if T has never been used.
     *
     * Implemented in terms of the const overload above (then const_cast-ing
     * the pointer back to mutable) rather than duplicating the lookup — safe
     * here because *this is genuinely non-const at the call site.
     */
    template <typename T>
    [[nodiscard]] pool_t<T>* FindPool() noexcept
    {
        return const_cast<pool_t<T>*>( std::as_const(*this).template FindPool<T>() );
    }

    /**
     * @brief Returns T's pool, creating (and registering) it on first use.
     */
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
     *
     * Returns a fresh Entity, or an error if the registry has no more room
     * (kMaxEntities alive at once).
     */
    [[nodiscard]] Result<Entity> CreateEntity() noexcept;

    /**
     * @brief Lists every currently-alive entity, regardless of its components.
     *
     * Unlike View<Ts...>, which only sees entities present in a specific
     * set of component pools, this walks the allocator directly — the
     * only way to reach an entity with no components yet, or to visit
     * every entity for something like scene serialization.
     */
    [[nodiscard]] std::vector<Entity> AllEntities() const noexcept;

    /**
     * @brief Destroys an entity and strips it from every component pool.
     *
     * Returns an error if inEntity is not currently alive. Its children, if
     * any, are not destroyed (see DestroyEntityGraph for that).
     */
    [[nodiscard]] BoolResult DestroyEntity( Entity inEntity ) noexcept;

    /**
     * @brief Destroys every entity and strips them from every component pool.
     *
     * Logs an error when it cannot remove a specific entity and carries on
     * with the rest.
     */
    void DestroyAllEntities() noexcept;

    /**
     * @brief Disables an entity by adding the Disable marker to it alone.
     *
     * No effect on its children -- IsDisabled() derives their disabled state
     * by walking up to this entity, so nothing needs writing to them.
     */
    void DisableEntity( Entity inEntity ) noexcept;

    /**
     * @brief True if inEntity or any of its ancestors carries the Disable marker.
     *
     * Walks up via components::Hierarchy::m_Parent, so the cost is O(depth).
     */
    bool IsDisabled( Entity inEntity ) const noexcept;

    /**
     * @brief Attaches inComponent to inEntity, creating T's pool on first use.
     *
     * Replaces an existing T on the entity. Returns a reference to the stored
     * component, or an error if inEntity's index is out of range or T's pool
     * is full.
     */
    template<typename T>
    Result<std::reference_wrapper<T>> AddComponent( Entity inEntity, T inComponent )
    {
        return GetOrCreatePool<T>().Insert( inEntity, std::move( inComponent ) );
    }

    /**
     * @brief Detaches inEntity's component of type T, if any.
     *
     * Returns an error if T has never been used or inEntity has no component
     * of this type.
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
     *
     * Returns a const reference to the component, or an error if T has never
     * been used or inEntity has none.
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
     *
     * Returns a reference to the component, or an error if T has never been
     * used or inEntity has none. The reference is invalidated by removing
     * components of the same type.
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

    /**
     * @brief Checks whether inEntity currently has a component of type T.
     *
     * False when T has never been used, instead of an error.
     */
    template <typename T>
    [[nodiscard]] bool HasComponent( Entity inEntity ) const noexcept
    {
        pool_t<T> const* pool = FindPool<T>();
        return pool && pool->Contains( inEntity );
    }

    /**
     * @brief Gets inEntity's component of type T, adding a default one first if missing.
     *
     * Marked noexcept but the add can fail (full pool or an invalid entity),
     * in which case the Value() read afterwards asserts.
     */
    template<typename T>
    [[nodiscard]] std::reference_wrapper<T> GetOrAddComponent( Entity inEntity ) noexcept
    {
        if ( !HasComponent<T>( inEntity ) ) AddComponent<T>( inEntity, T{} );
        return GetComponent<T>( inEntity ).Value();
    }

    /**
     * @brief Returns a lazy view over entities that have every component in Ts.
     *
     * Looks up each type's pool via FindPool — no pool is created for a
     * type that has never been used — so the result may be empty if any
     * of Ts has never been added to an entity yet. Entities IsDisabled()
     * considers disabled are skipped unless Ts includes markers::Disable
     * or the caller chains View::IncludeDisabled() onto the result. The view
     * yields (Entity, Ts&...) for each match.
     */
    template<typename ... Ts>
    [[nodiscard]] asge::ecs::View<Ts...> View() noexcept
    {
        return asge::ecs::View<Ts...>(
            [this]( Entity inEntity ){ return IsDisabled( inEntity ); }, FindPool<Ts>()...
        );
    }

    /**
     * @brief Read-only counterpart to View().
     *
     * Same lookup (including the IsDisabled() skip), but returns
     * View<Ts const...>, so iterating yields (Entity, Ts const&...) instead of
     * mutable references. The only overload callable through a Registry const&.
     */
    template<typename ... Ts>
    [[nodiscard]] asge::ecs::View<Ts const...> View() const noexcept
    {
        return asge::ecs::View<Ts const...>(
            [this]( Entity inEntity ){ return IsDisabled( inEntity ); }, FindPool<Ts>()...
        );
    }

    /**
     * @brief Stores inValue as the registry-wide instance of type T.
     *
     * Replaces any previous one. Unlike components, a resource is not attached to any Entity — there is
     * at most one T per Registry, found by its type alone. Not noexcept:
     * both growing m_Resources and constructing T's ResourceHolder may
     * allocate (or, for the latter, throw from T's own move constructor).
     */
    template<typename T>
    void SetResource( T inValue )
    {
        std::size_t const id = ResourceId<T>();
        if ( id >= m_Resources.size() ) m_Resources.resize( id + 1 );
        m_Resources[id] = std::make_unique<ResourceHolder<T>>( std::move(inValue) );
    }

    /**
     * @brief Looks up the registry-wide instance of type T.
     *
     * Returns a reference to the resource, or an error (EcsError::ResourceNotSet)
     * if SetResource<T>() has never been called on this registry.
     */
    template<typename T>
    [[nodiscard]] Result<std::reference_wrapper<T>> GetResource() noexcept
    {
        std::size_t const id = ResourceId<T>();
        if ( id >= m_Resources.size() || !m_Resources[id] )
        {
            return Result<std::reference_wrapper<T>>::Err(
                make_error_code( errors::EcsError::ResourceNotSet )
            );
        }

        auto& resource = static_cast<ResourceHolder<T>*>( m_Resources[id].get() )->m_Value;
        return Result<std::reference_wrapper<T>>::Ok(std::reference_wrapper<T>( resource ));
    }

    /**
     * @brief Calls the callback once for every alive entity.
     *
     * Iterates a snapshot taken by AllEntities(), so the callback may create
     * or destroy entities without invalidating the loop.
     */
    void ForEachEntity( std::function<void(Entity const&)> m_Callback ) const;
};

}

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
#include "ResourceHolder.hpp"

namespace asge::ecs
{

/**
 * @brief Owns entities, their components, and registry-wide resources; the
 *        ECS's single entry point.
 *
 * Wraps an EntityAllocator<kMaxEntities> for entity lifecycle and one
 * ComponentPool<T> per component type used so far (created lazily on the
 * first AddComponent<T>) and ties them together: DestroyEntity() strips
 * the entity from every pool, and View<Ts...>() iterates entities that
 * have every one of Ts across their pools. SetResource<T>/GetResource<T>
 * hold at most one T not tied to any Entity — e.g. shared/global state a
 * system needs but that isn't itself part of the world.
 */
class Registry
{
    EntityAllocator<kMaxEntities>                 m_Allocator{};
    std::vector<std::unique_ptr<IComponentPool>>  m_Pools{};
    std::vector<std::unique_ptr<IResourceHolder>> m_Resources{}; // dense, indexed by ResourceId<T>()

    // Next unused resource id, shared by every Registry instance -- same
    // "dense, monotonically-assigned, unsynchronized" tradeoff as
    // ComponentTypeId.hpp's GetComponentTypeId, just kept private to Registry
    // since resource ids (unlike component type ids) have no other caller.
    static inline std::size_t s_NextResourceId = 0;

    // Concrete pool type backing component type T.
    template<typename T>
    using pool_t = ComponentPool<T, component_cap_v<T>>;

    // Returns the stable id for resource type T, assigned on first use and
    // cached in a function-local static -- see s_NextResourceId's warning
    // above about assignment order/thread-safety.
    template <typename T>
    [[nodiscard]] static std::size_t ResourceId() noexcept
    {
        static std::size_t const id = s_NextResourceId++;
        return id;
    }

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
    void DestroyAllEntities() noexcept;

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

    /**
     * @brief Stores inValue as the registry-wide instance of type T,
     *        replacing any previous one.
     *
     * Unlike components, a resource is not attached to any Entity — there is
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
     * @return A reference to the resource, or an error if SetResource<T>()
     *         has never been called on this registry.
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
};

}

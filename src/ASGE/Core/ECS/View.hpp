#pragma once

#include <tuple>
#include <ASGE/Core/TraitFunctions.hpp>
#include <ASGE/Core/Functools.hpp>
#include "Entity.hpp"
#include "ComponentPool.hpp"

namespace asge::ecs
{

/**
 * @brief Lazy, non-owning iterator over entities that have every component in Ts.
 *
 * Built from one ComponentPool<T>* per T in Ts (typically via
 * Registry::View<Ts...>()); iterating yields (Entity, Ts&...) for every
 * entity present in all of them. Iteration walks the smallest contributing
 * pool's dense storage and filters each entity against the rest, so its
 * order follows that pool's storage order and can change after a Remove()
 * on any contributing pool.
 *
 * @warning If a requested component type has never been used in the
 *          Registry the view was built from, its pool pointer is null and
 *          the view is permanently empty — even if that type is added
 *          to an entity afterwards.
 * @warning Holds raw pointers into the pools it was built from; must not
 *          outlive the Registry (or ComponentPool instances) that produced them.
 *
 * @tparam Ts Component types an entity must have to appear in this view.
 */
template<typename ... Ts>
class View
{
public:
    using value_type = std::tuple<Entity, std::reference_wrapper<Ts>...>;
private:
    template<typename T> using Pool = ComponentPool<T, component_cap_v<T>>;
    
    using Tuple_t   = std::tuple<Pool<Ts>*...>;
    using Variant_t = std::variant<Pool<Ts>*...>;

    Tuple_t         m_Pools;         // Pointers to every pool contributing to the View
    bool            m_Valid;         // false if any pool is nullptr
    IComponentPool* m_SmallestPool;  // The least dense pool, drives iteration

    // Forward iterator over (Entity, Ts&...) tuples for entities present in
    // every pool in m_ActivePools; steps through the smallest pool's dense
    // storage, skipping entities the other pools don't also contain.
    class Iterator
    {
        Tuple_t         m_ActivePools;
        IComponentPool* m_SmallestPool;
        std::size_t     m_Index{};

        // True only if every pool in m_ActivePools contains this entity.
        bool PassesAllPools( Entity entity ) const
        {
            return std::apply( 
                [&]( auto*... pools ) {return ( pools->Contains(entity) && ... );}, 
                m_ActivePools
            );
        }

        // Advance m_Index until it lands on a matching entity, or falls off the end.
        void SkipToValid()
        {
            // No smallest pool means the owning View is invalid (see class
            // docs): nothing to iterate, so leave m_Index untouched.
            if ( !m_SmallestPool ) return;

            std::size_t const size = m_SmallestPool->Size();
            while ( m_Index < size && !PassesAllPools( m_SmallestPool->Entities()[m_Index] ) )
            {
                ++m_Index;
            }
        }

    public:
        Iterator( Tuple_t const& inPools, IComponentPool* inSmallestPool, std::size_t inIndex )
            : m_ActivePools( inPools ), m_SmallestPool( inSmallestPool )
            , m_Index( inIndex )
        {
            SkipToValid();
        }

        value_type operator*()
        {
            Entity const entity = m_SmallestPool->Entities()[m_Index];
            auto components = functools::MapTuple( 
                m_ActivePools,
                [&]( auto* pool ){ return std::ref( pool->Get(entity).Value() ); }
            );
            
            return functools::PrependToTuple( entity, components );
        }

        Iterator& operator++() 
        {
            ++m_Index;
            SkipToValid();
            return *this;
        }

        bool operator!=(Iterator const& other) const { return m_Index != other.m_Index; }
    };

    /** @brief Returns the smallest pool based on entity density */
    IComponentPool* GetSmallestPool() const noexcept
    {
        return std::apply([](auto* first, auto*... rest) -> IComponentPool*
        {
            IComponentPool* smallest = first;
            ( (rest->Size() < smallest->Size() ? (smallest = rest) : smallest), ... );
            return smallest;
        }, m_Pools);
    }

public:
    /**
     * @brief Builds a view over the given per-type component pools.
     *
     * inPools[i] should be the pool for the i-th type in Ts (nullptr if
     * that type has never been used), typically Registry::FindPool<T>()'s
     * result — Registry::View<Ts...>() is the intended way to construct this.
     */
    View( Pool<Ts>*&& ... inPools )
        : m_Pools(std::forward<Pool<Ts>*>(inPools)...)
        , m_Valid(!_internal::traits::has_nullptr(m_Pools))
        , m_SmallestPool( m_Valid ? GetSmallestPool() : nullptr )
    {
    }

    /** @brief Start of the view; matches end() immediately if any pool is missing. */
    Iterator begin()
    {
        if ( !m_Valid ) return end();
        return Iterator( m_Pools, m_SmallestPool, 0 );
    }

    /** @brief One-past-the-last element of the view. */
    Iterator end()
    {
        std::size_t const size = m_Valid ? m_SmallestPool->Size() : 0;
        return Iterator( m_Pools, m_SmallestPool, size );
    }
};

}
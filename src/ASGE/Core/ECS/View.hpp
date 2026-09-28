#pragma once

#include <tuple>
#include <type_traits>
#include <ASGE/Core/TraitFunctions.hpp>
#include <ASGE/Core/Functools.hpp>
#include "Entity.hpp"
#include "ComponentPool.hpp"
#include "Tags.hpp"

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
 * An entity carrying components::DisableTag is skipped during iteration by
 * default -- call IncludeDisabled() to see it anyway. This skip does not
 * apply if DisableTag itself is one of Ts, since the caller is then
 * explicitly asking to see disabled entities.
 *
 * Ts may be const-qualified (e.g. `View<Sprite const>`, what
 * Registry::View<Ts...>() const returns) to get (Entity, Ts const&...)
 * instead — the same pools, just read-only, for a Registry the caller only
 * holds a const reference to.
 *
 * @tparam Ts Component types an entity must have to appear in this view.
 */
template<typename ... Ts>
class View
{
public:
    using value_type = std::tuple<Entity, std::reference_wrapper<Ts>...>;
private:
    // Pool<T> is const-qualified whenever T is, so a const Ts yields a
    // ComponentPool<T,Cap> const* -- Cap is always looked up via T's
    // unqualified form, so this names the exact same pool type
    // Registry::pool_t<T> does regardless of Ts's constness here.
    template<typename T> using Pool = std::conditional_t<
        std::is_const_v<T>,
        ComponentPool<std::remove_const_t<T>, component_cap_v<std::remove_const_t<T>>> const,
        ComponentPool<T, component_cap_v<T>>
    >;

    using Tuple_t   = std::tuple<Pool<Ts>*...>;
    using Variant_t = std::variant<Pool<Ts>*...>;

    static constexpr bool kSkipDisabled = !( std::is_same_v<std::remove_const_t<Ts>, components::DisableTag> || ... );

    IComponentPool const* m_DisabledPool;    // Pool with entities associated a Disable Flag
    Tuple_t               m_Pools;           // Pointers to every pool contributing to the View
    bool                  m_Valid;           // false if any pool is nullptr
    IComponentPool const* m_SmallestPool;    // The least dense pool, drives iteration -- only ever read from, so const regardless of Ts
    
    bool m_IncludeDisabled{false}; // Set when the called would like to include also disabled entities

    class Iterator; // Forward declaration
    friend class Iterator; // Now it can access private View methods

    // Forward iterator over (Entity, Ts&...) tuples for entities present in
    // every pool in m_ActivePools; steps through the smallest pool's dense
    // storage, skipping entities the other pools don't also contain.
    class Iterator
    {
        View const* m_View;
        std::size_t m_Index{};

        // True only if every pool in m_ActivePools contains this entity.
        bool PassesAllPools( Entity entity ) const
        {
            bool selector {true};
            if constexpr ( View::kSkipDisabled )
            {
                if (   !m_View->m_IncludeDisabled && m_View->m_DisabledPool 
                    &&  m_View->m_DisabledPool->Contains( entity ) )
                {
                    selector = false;
                }
            } else {}

            return selector && std::apply( 
                [&]( auto*... pools ) {return ( pools->Contains(entity) && ... );}, 
                m_View->m_Pools
            );
        }

        // Advance m_Index until it lands on a matching entity, or falls off the end.
        void SkipToValid()
        {
            // No smallest pool means the owning View is invalid (see class
            // docs): nothing to iterate, so leave m_Index untouched.
            if ( !m_View->m_SmallestPool ) return;

            std::size_t const size = m_View->m_SmallestPool->Size();
            while ( m_Index < size && !PassesAllPools( m_View->m_SmallestPool->Entities()[m_Index] ) )
            {
                ++m_Index;
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = View::value_type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = value_type;

        Iterator(View const* inView, std::size_t inIndex) 
        : m_View( inView ), m_Index( inIndex )
        {
            SkipToValid();
        }

        value_type operator*()
        {
            Entity const entity = m_View->m_SmallestPool->Entities()[m_Index];
            auto components = functools::MapTuple( 
                m_View->m_Pools,
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
        bool operator==(Iterator const& other) const { return !(m_Index != other.m_Index); }
    };

    /** @brief Returns the smallest pool based on entity density */
    IComponentPool const* GetSmallestPool() const noexcept
    {
        return std::apply([](auto* first, auto*... rest) -> IComponentPool const*
        {
            IComponentPool const* smallest = first;
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
    View( IComponentPool const* inDisabledPool, Pool<Ts>*&& ... inPools )
        : m_DisabledPool(inDisabledPool)
        , m_Pools(std::forward<Pool<Ts>*>(inPools)...)
        , m_Valid(!_internal::traits::has_nullptr(m_Pools))
        , m_SmallestPool( m_Valid ? GetSmallestPool() : nullptr )
    {
    }

    /** @brief Start of the view; matches end() immediately if any pool is missing. */
    Iterator begin()
    {
        if ( !m_Valid ) return end();
        return Iterator( this, 0 );
    }

    /** @brief One-past-the-last element of the view. */
    Iterator end()
    {
        std::size_t const size = m_Valid ? m_SmallestPool->Size() : 0;
        return Iterator( this, size );
    }

    /**
     * @brief Opts this view back into entities carrying components::DisableTag.
     * @return *this, moved -- chain directly off Registry::View(), e.g.
     *         `for (auto ... : registry.View<T>().IncludeDisabled())`.
     */
    View IncludeDisabled() && noexcept
    {
        m_IncludeDisabled = true;
        return std::move( *this );
    }
};

}
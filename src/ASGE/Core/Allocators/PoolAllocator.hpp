#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <array>
#include <new>
#include <type_traits>

namespace asge::mem
{

/**
 * @brief Fixed-capacity pool allocator backed by stack storage.
 *
 * Manages a static array of N slots, each sized and aligned for T. Slots are
 * linked in a free-list; allocation and deallocation are both O(1).
 *
 * Ownership is expressed through a @c pointer (a @c std::unique_ptr with a
 * custom deleter). When a @c pointer is destroyed the slot is automatically
 * returned to the pool and the object's destructor is called.
 *
 * The allocator must be created via @c Create() so that its lifetime is
 * managed by a @c shared_ptr. The deleter of each live @c pointer holds a
 * @c shared_ptr back to the pool, ensuring the pool outlives all of its
 * outstanding allocations even if the caller drops their own @c shared_ptr.
 *
 * Copy and move are disabled — the internal slot addresses must remain stable.
 *
 * @tparam T Element type stored in each pool slot.
 * @tparam N Number of slots; fixed at compile time.
 */
template<typename T, std::size_t N>
class PoolAllocator : public std::enable_shared_from_this<PoolAllocator<T, N>>
{
private:
    /** One pool entry: either live object storage or a free-list pointer. */
    union Slot
    {
        alignas(T) std::byte s_Storage[sizeof(T)];
        Slot* s_Next{nullptr};
    };

    /**
     * @brief Custom deleter injected into every @c pointer returned by @c Alloc.
     *
     * Holds a @c shared_ptr to the pool so the pool stays alive for as long as
     * any outstanding allocation exists.
     */
    struct Deleter
    {
        std::shared_ptr<PoolAllocator> s_Pool;
        inline void operator()( T* inPtr )
        {
            s_Pool->Free( inPtr );
            s_Pool.reset();
        }
    };

    std::array<Slot, N> m_Pool;  // Stack-allocated slot array.
    Slot* m_FreeHead{nullptr};   // Head of the intrusive free-list.
    std::size_t m_Used{0};       // Total number of used slots

    PoolAllocator()
    {
        for ( size_type ii = 0; ii < N - 1; ++ii )
        {
            m_Pool[ii].s_Next = &m_Pool[ii + 1];
        }

        m_Pool[N - 1].s_Next = nullptr;
        m_FreeHead = &m_Pool[0];
    }

    /**
     * @brief Returns a slot to the free-list and destructs the held object.
     * @note Called exclusively by @c Deleter — not part of the public API.
     */
    void Free( T* inPtr ) noexcept
    {
        inPtr->~T();

        Slot* slot   = reinterpret_cast<Slot*>( inPtr );
        slot->s_Next = m_FreeHead;
        m_FreeHead   = slot;
        
        m_Used--;
    }

public:
    using size_type  = std::size_t;
    using slot_type  = Slot;
    using value_type = T;
    using pointer    = std::unique_ptr<T, Deleter>;

    PoolAllocator(const PoolAllocator&)            = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&)                 = delete;
    PoolAllocator& operator=(PoolAllocator&&)      = delete;

    /**
     * @brief Creates a pool allocator managed by a @c shared_ptr.
     *
     * The constructor is private; this factory is the only way to obtain an
     * instance so that @c shared_from_this works correctly.
     *
     * @return A @c shared_ptr owning the new pool.
     */
    [[nodiscard]] static std::shared_ptr<PoolAllocator> Create()
    {
        return std::shared_ptr<PoolAllocator>( new PoolAllocator() );
    }

    /**
     * @brief Constructs a @c T in the next free slot and returns an owning handle.
     *
     * Returns a null @c pointer when the pool is exhausted (all N slots are in
     * use). The returned @c pointer's deleter keeps the pool alive independently
     * of the caller's @c shared_ptr.
     *
     * @tparam _Args Constructor argument types for @c T.
     * @param  inArgs Arguments forwarded to @c T's constructor.
     * @return An owning @c pointer to the constructed object, or @c nullptr.
     */
    template<typename ..._Args>
    requires std::is_constructible_v<T, _Args...>
    [[nodiscard]] pointer Alloc(_Args&& ...inArgs) noexcept
    {
        if ( m_FreeHead == nullptr ) return { nullptr, Deleter{ nullptr } };

        Slot* currSlot = m_FreeHead;
        m_FreeHead     = m_FreeHead->s_Next;

        T* retObj = reinterpret_cast<T*>( currSlot->s_Storage );
        ::new(retObj) T( std::forward<_Args>(inArgs)... );
        m_Used++;

        return { retObj, Deleter{ this->shared_from_this() } };
    }

    [[nodiscard]] size_type Used() const noexcept { return m_Used; }
    [[nodiscard]] size_type Remaining() const noexcept { return N - m_Used; }
    [[nodiscard]] size_type Full() const noexcept { return m_Used == N; }
    [[nodiscard]] size_type Empty() const noexcept { return m_Used == 0; }
};

}
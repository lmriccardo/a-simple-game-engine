#pragma once

#include <cstdint>
#include <atomic>
#include <array>

namespace asge::concurrent
{

namespace _internal
{

/**
 * @brief A lock-free, bounded, multiple-producer multiple-consumer queue.
 *
 * Implemented as a ring buffer of N slots, where each slot carries its own
 * sequence number acting as a per-slot turn indicator between producers and
 * consumers. Coordination is achieved entirely through atomic operations —
 * no mutexes, no condition variables.
 *
 * Producers and consumers race to claim slots via CAS on a shared head index.
 * A slot is only considered ready to read after the producer that claimed it
 * has finished writing and published the sequence increment via a release store.
 * This prevents consumers from observing partially written data even when
 * multiple producers are writing concurrently.
 *
 * N is the capacity of the queue and must be a power of 2; T is the stored
 * element type and must be move-assignable. TryPush and TryPop never allocate
 * and are wait-free on the fast path.
 */
template<std::size_t N, typename T>
class MPMCQueue
{
    static_assert( (N & ( N - 1 )) == 0, "N must be a power of 2" );

    struct Slot
    {
        T                  s_Item;           // The stored element
        std::atomic_size_t s_Sequence{0};    // Turn indicator for producers and consumers
    };

    std::array<Slot, N> m_Slots;         // The ring buffer
    std::atomic_size_t  m_WriteHead{0};  // Next position producers claim
    std::atomic_size_t  m_ReadHead{0};   // Next position consumers claim
public:
    using value_type   = T;
    using reference    = T&;
    using size_type    = std::size_t;
    using pointer_diff = std::ptrdiff_t;

    /**
     * @brief Creates an empty queue by giving slot i the initial sequence i.
     */
    MPMCQueue()
    {
        for ( size_type ii = 0; ii < N; ++ii )
        {
            m_Slots[ii].s_Sequence.store(ii, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Tries to append inItem; returns false if the queue is full.
     *
     * Never blocks. On failure inItem is not stored (it may already have
     * been moved from, so do not rely on its value).
     */
    bool TryPush(value_type inItem) noexcept
    {
        size_type write = m_WriteHead.load(std::memory_order_relaxed);

        while ( true )
        {
            Slot& currSlot = m_Slots[ write & ( N - 1 ) ];
            size_type seq = currSlot.s_Sequence.load(std::memory_order_acquire);
            pointer_diff diff = static_cast<pointer_diff>(seq - write);

            if ( diff < 0 ) return false;
            else if ( diff == 0 )
            {
                // The slot is free, we could try to claim it. If the condition returns True
                // then the current producer has claimed the slot and can write data into it
                // by also updating its sequence counter. On the other hand, if the condition
                // has returned false, there is another producer that has previously claimed
                // this slot concurrently, the write index increments by 1 and it tries in
                // the next loop to acquire the next slot.
                if ( m_WriteHead.compare_exchange_weak(
                    write, write + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed
                ) ) {
                    currSlot.s_Item = std::move(inItem);
                    currSlot.s_Sequence.store( write + 1, std::memory_order_release );
                    return true;
                }
            }
            else
            {
                // Another producer published this slot after we read `write` but before
                // we reached the CAS. Our local `write` is stale — reload it.
                write = m_WriteHead.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Tries to take the oldest element into outItem; returns false if empty.
     *
     * Never blocks. outItem is only written on success.
     */
    bool TryPop(reference outItem) noexcept
    {
        size_type read = m_ReadHead.load( std::memory_order_relaxed );

        while ( true )
        {
            Slot& currSlot = m_Slots[ read & ( N - 1 ) ];
            size_type seq = currSlot.s_Sequence.load(std::memory_order_acquire);
            pointer_diff diff = static_cast<pointer_diff>(seq - ( read + 1 ));

            if ( diff < 0 ) return false;
            else if ( diff == 0 )
            {
                if ( m_ReadHead.compare_exchange_weak(
                    read, read + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed
                ) ) {
                    outItem = std::move(currSlot.s_Item);
                    currSlot.s_Sequence.store( read + N, std::memory_order_release );
                    return true;
                }
            }
            else
            {
                // Another consumer published this slot after we read `read` but before
                // we reached the CAS. Our local `read` is stale — reload it.
                read = m_ReadHead.load(std::memory_order_relaxed);
            }
        }
    }
};

}

}
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <ASGE/Core/Errors.hpp>
#include <system_error>

namespace asge::mem
{

/**
 * @brief Fixed-capacity index allocator backed by an intrusive free-list.
 *
 * Hands out and reclaims indices in the range @c [0, N) in O(1), with no
 * heap allocation. The @c N slots are pre-linked into a singly-linked free
 * chain at construction; @c Get() pops the head of the chain and @c Free()
 * pushes an index back onto it (LIFO reuse order). The chain is terminated
 * by the sentinel value @c N (not a real slot): once @c m_FreeHead equals
 * @c N every slot is in use and @c Get() reports exhaustion.
 *
 * Each slot also tracks whether it is currently in use, so @c Free() can
 * reject indices that were never handed out or have already been freed,
 * guarding against double-frees.
 *
 * This class does not store any data itself — it only manages indices. It
 * is meant to be paired with an external, equally-sized container (e.g. a
 * sparse array) that the returned indices key into.
 *
 * @tparam N Number of allocatable slots; fixed at compile time.
 */
template<std::size_t N>
class FreeList
{
    struct node_info
    {
        std::size_t m_NextNode; // Next free node in the free list
        bool        m_InUse;    // If the current node is in use or not
    };

    std::array<node_info, N> m_NodeList{}; // A List of all nodes
    std::size_t m_FreeHead{0}; // The current free position

public:
    FreeList()
    {
        for ( std::size_t ii = 0; ii < N; ++ii )
        {
            m_NodeList[ii] = node_info{ ii + 1, false };
        }
    }

    FreeList(FreeList const&) = delete;
    FreeList(FreeList&&) = default;
    FreeList& operator=(FreeList const&) = delete;
    FreeList& operator=(FreeList&&) = default;

    ~FreeList() = default;

    /**
     * @brief Claims the next free index and marks it in use.
     *
     * @return The claimed index on success, or an error with
     *         @c std::errc::result_out_of_range if all @c N slots are in use.
     */
    [[nodiscard]] Result<std::size_t> Get() noexcept
    {
        if (m_FreeHead == N)
        {
            return Result<std::size_t>::Err(std::make_error_code(std::errc::result_out_of_range));
        }

        std::size_t result = m_FreeHead;
        node_info& node = m_NodeList[m_FreeHead];
        node.m_InUse = true;
        m_FreeHead = node.m_NextNode;

        return Result<std::size_t>::Ok(result);
    }

    /**
     * @brief Checks whether an index is currently claimed via @c Get().
     *
     * @param inIndex Index to check.
     * @return @c true if @c inIndex is in range and currently in use.
     */
    [[nodiscard]] bool IsUsed(std::size_t inIndex) const noexcept
    {
        return inIndex < N && m_NodeList[inIndex].m_InUse;
    }

    /**
     * @brief Releases a previously-claimed index back to the free-list.
     *
     * @param inIndex Index previously returned by @c Get().
     * @return Ok on success, or an error with @c std::errc::invalid_argument
     *         if @c inIndex is out of range, was never claimed, or has
     *         already been freed.
     */
    BoolResult Free(std::size_t inIndex) noexcept
    {
        if (!IsUsed( inIndex ))
        {
            return BoolResult::Err(std::make_error_code(std::errc::invalid_argument));
        }

        node_info& node = m_NodeList[inIndex];
        node.m_InUse = false;
        node.m_NextNode = m_FreeHead;
        m_FreeHead = inIndex;

        return BoolResult::Ok();
    }
};

}
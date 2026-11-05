#pragma once

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <memory>
#include <new>
#include <type_traits>

#define DEF_ALIGN alignof( std::max_align_t)

namespace asge::mem
{

namespace _internal
{

/**
 * @brief Rounds inValue up to the next multiple of inAlignment.
 *
 * inAlignment must be a power of two; this is asserted in debug builds
 * and gives a wrong result otherwise. An already aligned value is returned as is.
 */
std::size_t AlignUp( std::size_t inValue, std::size_t inAlignment ) noexcept;

}

/**
 * @brief Arena allocator that bumps a pointer through a fixed-size buffer.
 *
 * Allocations are O(1) and never fragment — the offset only moves forward.
 * Memory can be reclaimed in bulk via Reset() or rolled back to a saved
 * Marker via Rewind(). Individual frees are not supported.
 * 
 * It does not own the buffer, hence it must be created outside of the allocator.
 */
class LinearAllocator
{
private:
    std::byte*  m_Base;       // Start of the borrowed buffer
    std::size_t m_Size;       // Buffer size in bytes
    std::size_t m_Offset{0};  // Bump offset: bytes consumed so far

    struct Marker { std::size_t s_Offset; }; // Saved offset to rewind to
public:
    using size_type    = std::size_t;
    using pointer_type = std::byte*;
    using marker_type  = Marker;

    /**
     * @brief Creates an allocator with no buffer.
     *
     * Its members are uninitialised until it is move-assigned from a real
     * allocator, so do not allocate from it before that.
     */
    LinearAllocator() = default;

    /**
     * @brief Creates an allocator that hands out memory from inBuffer.
     *
     * The buffer is borrowed, not owned: it must stay valid for the
     * allocator's lifetime and be at least inSize bytes.
     */
    LinearAllocator( void* inBuffer, std::size_t inSize );

    /**
     * @brief Copy construction is disabled.
     */
    LinearAllocator( LinearAllocator const& )            = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    LinearAllocator& operator=( LinearAllocator const& ) = delete;

    /**
     * @brief Takes over inOther's buffer and offset, leaving it empty.
     */
    LinearAllocator( LinearAllocator&& inOther );

    /**
     * @brief Move-assigns inOther's buffer and offset, leaving it empty.
     */
    LinearAllocator& operator=( LinearAllocator&& inOther);

    /**
     * @brief Allocates inBytes from the buffer with the given alignment
     *
     * Rounds the current offset up to inAlign (a power of two, defaulting to
     * the platform's maximum alignment), then bumps it by inBytes. Returns
     * nullptr, without changing the offset, if the buffer cannot fit it.
     * The memory is uninitialised.
     */
    [[nodiscard]]
    void* Alloc( size_type inBytes, size_type inAlign = DEF_ALIGN ) noexcept;

    /**
     * @brief Allocates memory for one object of type T
     * 
     * It allocates memory for one object of type T and constructs it
     * in-place using placement new and input arguments. Alignment is
     * automatically derived from `alignof(T)`. Throws std::bad_alloc if the
     * allocator is full. The destructor is never run: the allocator only
     * reclaims bytes, so this suits trivially destructible types.
     */
    template<typename T, typename ..._Args> 
    requires std::is_constructible_v<T, _Args...>
    [[nodiscard]] T* AllocOne( _Args&& ... inArgs )
    {
        void *mem = Alloc( sizeof(T), alignof(T) );
        if (!mem) throw std::bad_alloc{};
        return ::new(mem) T( std::forward<_Args>(inArgs)... );
    }

    /**
     * @brief Allocates an array of objects of type T
     * 
     * Allocates a contiguous array of inCount objects of type T. For non-trivial
     * types, each element is default-constructed via placement new. For trivial
     * types (int, float, POD structs), construction is skipped entirely for
     * maximum speed, so the elements hold indeterminate values. Alignment comes
     * from alignof(T) and std::bad_alloc is thrown if the arena is full.
     */
    template<typename T> [[nodiscard]]
    T* AllocArray( size_type inCount )
    {
        void* mem = Alloc( sizeof(T) * inCount, alignof(T) );
        if (!mem) throw std::bad_alloc{};
        T* outArr = static_cast<T*>( mem );
        if constexpr (!std::is_trivially_constructible_v<T>)
        {
            for ( std::size_t ii = 0; ii < inCount; ii++ )
            {
                ::new(outArr + ii) T{};
            }
        }

        return outArr;
    }

    /**
     * @brief Returns a checkpoint of the current allocator offset.
     *
     * Pass it to Rewind() later to release everything allocated after it.
     */
    [[nodiscard]] marker_type GetMarker() const noexcept;

    /**
     * @brief Rewinds the allocator to a given offset
     *
     * Everything allocated after inMarker was taken is released at once; no
     * destructors run. The marker must not be newer than the current offset,
     * which is asserted in debug builds.
     */
    void Rewind( marker_type inMarker );

    /**
     * @brief Performs a full reset by resetting the offset to 0.
     *
     * The whole buffer becomes available again; no destructors run.
     */
    void Reset() noexcept;

    /**
     * @brief Bytes consumed so far, including alignment padding.
     */
    [[nodiscard]] size_type Used() const noexcept;

    /**
     * @brief Bytes left in the buffer (ignores padding a future Alloc may need).
     */
    [[nodiscard]] size_type Remaining() const noexcept;

    /**
     * @brief Total size of the buffer in bytes.
     */
    [[nodiscard]] size_type Capacity() const noexcept;

    /**
     * @brief True while nothing has been allocated (offset is 0).
     */
    [[nodiscard]] bool      Empty() const noexcept;
};

/**
 * @brief Owning arena allocator built on top of LinearAllocator.
 *
 * Internally allocates a heap buffer of the requested size and passes it
 * to the parent LinearAllocator. Unlike LinearAllocator, ArenaAllocator
 * owns its storage and releases it automatically on destruction.
 * All LinearAllocator operations (Alloc, AllocOne, AllocArray, GetMarker,
 * Rewind, Reset) are available through inheritance.
 */
class ArenaAllocator : public LinearAllocator
{
private:
    std::unique_ptr<std::byte[]> m_Storage; // The owned heap buffer
public:
    using LinearAllocator::size_type;
    using LinearAllocator::marker_type;
    using LinearAllocator::pointer_type;

    /**
     * @brief Allocates a heap buffer of inSize bytes and manages it as an arena.
     *
     * The buffer is freed when the ArenaAllocator is destroyed.
     */
    ArenaAllocator( size_type inSize );

    /**
     * @brief Copy construction is disabled.
     */
    ArenaAllocator( ArenaAllocator const& )            = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    ArenaAllocator& operator=( ArenaAllocator const& ) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    ArenaAllocator( ArenaAllocator&& )            = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    ArenaAllocator& operator=( ArenaAllocator&& ) = default;

    /**
     * @brief Destructor.
     *
     * Defaulted; members clean up after themselves.
     */
    ~ArenaAllocator() = default;
};

/**
 * @brief Stack-allocated arena backed by an inline byte array of size N.
 *
 * The buffer lives on the stack (or as a member) — no heap allocation occurs.
 * Inherits all LinearAllocator operations. Useful for short-lived, bounded
 * allocations where heap overhead or latency is unacceptable. N is the
 * size of the backing buffer in bytes.
 */
template<std::size_t N>
class StackBuffer : public LinearAllocator
{
private:
    alignas(std::max_align_t) std::byte m_Buffer[N]; // The inline backing storage
public:
    /**
     * @brief Creates the allocator over its own inline buffer.
     *
     * Not safe to copy or move: the base class would keep pointing at the
     * old object's buffer.
     */
    StackBuffer() : LinearAllocator( m_Buffer, N ) {}
};

}
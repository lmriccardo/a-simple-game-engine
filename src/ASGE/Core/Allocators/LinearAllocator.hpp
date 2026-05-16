#pragma once

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <memory>
#include <new>

#define DEF_ALIGN alignof( std::max_align_t)

namespace asge::mem
{

namespace _internal
{

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
    std::byte*  m_Base;
    std::size_t m_Size;
    std::size_t m_Offset{0};

    struct Marker { std::size_t s_Offset; };
public:
    using size_type    = std::size_t;
    using pointer_type = std::byte*;
    using marker_type  = Marker;

    LinearAllocator() = default;
    LinearAllocator( void* inBuffer, std::size_t inSize );

    // Non-copyable but movable
    LinearAllocator( LinearAllocator const& )            = delete;
    LinearAllocator& operator=( LinearAllocator const& ) = delete;

    LinearAllocator( LinearAllocator&& inOther );
    LinearAllocator& operator=( LinearAllocator&& inOther);

    /**
     * @brief Allocates N bytes from the buffer with the given alignment
     * 
     * @param inBytes Number of bytes to allocate
     * @param inAlign Required address alignment ( default to platform alignment )
     * @return Pointer to the allocated region or nullptr
     */
    [[nodiscard]]
    void* Alloc( size_type inBytes, size_type inAlign = DEF_ALIGN ) noexcept;

    /**
     * @brief Allocates memory for one object of type T
     * 
     * It allocates memory for one object of type T and constructs it
     * in-place using placement new and input arguments. Alignment is
     * automatically derived from `alignof(T)`.
     * 
     * @tparam T Type to allocate and construct
     * @tparam _Args Constructor argument types (deduced)
     * 
     * @param inArgs Arguments forwarded to T's constructor.
     * @return Pointer to the fully constructed T object.
     * @throws std::bad_alloc if the allocator is full.
     */
    template<typename T, typename ..._Args> [[nodiscard]]
    T* AllocOne( _Args&& ... inArgs )
    {
        void *mem = Alloc( sizeof(T), alignof(T) );
        if (!mem) throw std::bad_alloc{};
        return ::new(mem) T( std::forward<_Args>(inArgs)... );
    }

    /**
     * @brief Allocates an array of objects of type T
     * 
     * Allocates a contiguous array of N objects of type T. For non-trivial 
     * types, each element is default-constructed via placement new. For trivial
     * types (int, float, POD structs), construction is skipped entirely for 
     * maximum speed.
     * 
     * Alignment is automatically derived from alignof(T).
     * 
     * @tparam T Element type.
     * 
     * @param  inCount  Number of elements.
     * @return Pointer to the first element of the array.
     * @throws std::bad_alloc if the arena is full.
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

    /* Returns like a checkpoint of the current allocator offset */
    [[nodiscard]] marker_type GetMarker() const noexcept;

    /**
     * @brief Rewinds the allocator to a given offset
     * 
     * @param inMarker A previously created checkpoint
     * @throw Assertion error if the marker is from the future.
     */
    void Rewind( marker_type inMarker );

    /* Performs a full reset by resetting the offset to 0 */
    void Reset() noexcept;

    [[nodiscard]] size_type Used() const noexcept;
    [[nodiscard]] size_type Remaining() const noexcept;
    [[nodiscard]] size_type Capacity() const noexcept;
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
    std::unique_ptr<std::byte[]> m_Storage;
public:
    using LinearAllocator::size_type;
    using LinearAllocator::marker_type;
    using LinearAllocator::pointer_type;

    ArenaAllocator( size_type inSize );

    ArenaAllocator( ArenaAllocator const& )            = delete;
    ArenaAllocator& operator=( ArenaAllocator const& ) = delete;

    ArenaAllocator( ArenaAllocator&& )            = default;
    ArenaAllocator& operator=( ArenaAllocator&& ) = default;

    ~ArenaAllocator() = default;
};

/**
 * @brief Stack-allocated arena backed by an inline byte array of size N.
 *
 * The buffer lives on the stack (or as a member) — no heap allocation occurs.
 * Inherits all LinearAllocator operations. Useful for short-lived, bounded
 * allocations where heap overhead or latency is unacceptable.
 *
 * @tparam N Size of the backing buffer in bytes.
 */
template<std::size_t N>
class StackBuffer : public LinearAllocator
{
private:
    alignas(std::max_align_t) std::byte m_Buffer[N];
public:
    StackBuffer() : LinearAllocator( m_Buffer, N ) {}
};

}
#pragma once

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <array>
#include <type_traits>

namespace asge::mem
{

/**
 * @brief A stack-based allocator that manages a fixed-size memory buffer.
 *
 * Allocates objects of any type sequentially from a contiguous buffer of N bytes.
 * Each allocation stores a Header, a back pointer, the object itself, and a footer,
 * allowing both individual LIFO frees and bulk rollback via markers.
 *
 * Memory layout per allocation: [ Header | backPtr | object | footer ]
 *
 * Constraints:
 *   - Non-copyable and non-movable. The allocator must outlive all pointers it returns.
 *   - Free(T*) must be called in strict LIFO order. Out-of-order frees are caught
 *     by an assert in debug builds.
 *   - FreeToMarker() and Reset() call destructors on all unwound objects automatically.
 *
 * @tparam N Total size of the memory buffer in bytes.
 */
template<std::size_t N>
class StackAllocator
{
private:
    /** Per-allocation metadata written just before each object in the buffer. */
    struct Header
    {
        std::size_t s_PrevTop;            // m_Top value before this allocation.
        std::size_t s_ObjStart;           // Where the object actually starts
        void      (*s_Destructor)(void*); // Type-erased destructor for the object.
    };

    std::array<std::byte, N> m_Buffer; // Raw byte buffer.
    std::size_t              m_Top{0}; // Offset one past the last live byte.

    /** Rounds @p inOffset up to the next multiple of @p inAlign (must be a power of two). */
    static std::size_t
    Align(std::size_t inOffset, std::size_t inAlign) noexcept
    {
        return (inOffset + inAlign - 1) & ~(inAlign - 1);
    }

    Header* GetHeader(void* inPtr) noexcept
    {
        std::byte*   objBytes    = reinterpret_cast<std::byte*>(inPtr);
        std::size_t* backPtr     = reinterpret_cast<std::size_t*>(objBytes) - 1;
        std::size_t  headerStart = *backPtr;
        return reinterpret_cast<Header*>(&m_Buffer[headerStart]);
    }

public:
    using size_type   = std::size_t;
    using marker_type = size_type; // A marker is the raw buffer offset at a point in time.

    StackAllocator()  = default;
    ~StackAllocator() { Reset(); }

    StackAllocator(const StackAllocator&)            = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&&)                 = delete;
    StackAllocator& operator=(StackAllocator&&)      = delete;

    /**
     * @brief Allocates and constructs an object of type T in the buffer.
     *
     * Computes the correct alignment for the Header, back pointer, object,
     * and footer before placing them sequentially in the buffer.
     *
     * @tparam T    Type of object to allocate.
     * @tparam Args Constructor argument types.
     * @param  inArgs Arguments forwarded to T's constructor.
     * @return Pointer to the constructed object, or nullptr if the buffer is exhausted.
     */
    template<typename T, typename... _Args>
    requires std::is_constructible_v<T, _Args...>
    [[nodiscard]] T* Alloc(_Args&& ...inArgs) noexcept
    {
        size_type headStart    = Align( m_Top, alignof(Header) );
        size_type backPtrStart = Align( headStart + sizeof(Header), alignof(std::size_t) );
        size_type objStart     = Align( backPtrStart + sizeof(std::size_t), alignof(T) );
        size_type objEnd       = objStart + sizeof(T);
        size_type footerStart  = Align( objEnd, alignof(std::size_t) );
        size_type newTop       = footerStart + sizeof(std::size_t);

        if ( newTop > N ) return nullptr;

        // Set the header information
        Header* header       = reinterpret_cast<Header*>(&m_Buffer[headStart]);
        header->s_PrevTop    = m_Top;
        header->s_ObjStart   = objStart;
        header->s_Destructor = [](void* ptr){ static_cast<T*>(ptr)->~T(); };

        // Set the backpointer information
        std::size_t* backPtr = reinterpret_cast<std::size_t*>(&m_Buffer[backPtrStart]);
        *backPtr             = headStart;
        std::size_t* footer  = reinterpret_cast<std::size_t*>(&m_Buffer[footerStart]);
        *footer              = headStart;

        m_Top = newTop;

        // Construct the object in place and returns the pointer
        T* obj = reinterpret_cast<T*>( &m_Buffer[objStart] );
        ::new(obj) T( std::forward<_Args>(inArgs)...);
        return obj;
    }

    /** Returns the current top offset as a rewind checkpoint. */
    [[nodiscard]] marker_type GetMarker() const noexcept { return m_Top; }

    /**
     * @brief Destroys and frees the most recently allocated object.
     *
     * Calls T's destructor and restores m_Top to its value before this allocation.
     * Must be called in strict LIFO order — asserts if inPtr is not the most
     * recent allocation.
     *
     * @tparam T   Type of the object to free.
     * @param inPtr Pointer previously returned by Alloc<T>(). Must not be nullptr.
     */
    template<typename T>
    void Free( T* inPtr ) noexcept
    {
        assert( inPtr != nullptr && "cannot free a null pointer" );

        size_type objStart = reinterpret_cast<std::byte*>( inPtr ) - m_Buffer.data();
        assert(objStart < m_Top && "freeing out of order — not the most recent allocation!");

        Header* header = GetHeader(inPtr);
        header->s_Destructor(inPtr);
        m_Top = header->s_PrevTop;
    }

    /**
     * @brief Destroys all objects allocated after the given marker and rolls back the stack.
     *
     * Walks the allocation chain backwards from m_Top to inMarker, calling each
     * object's destructor in reverse allocation order.
     *
     * @param inMarker A value previously returned by GetMarker(). Must be <= m_Top.
     */
    void FreeToMarker(marker_type inMarker) noexcept
    {
        assert(inMarker <= m_Top && "marker is above current top");

        while ( m_Top > inMarker )
        {
            // footer is always the last sizeof(std::size_t) bytes of the block
            size_type  footerStart = m_Top - sizeof(std::size_t);
            size_type* footer      = reinterpret_cast<std::size_t*>(&m_Buffer[footerStart]);
            size_type  headerStart = *footer;

            Header* header = reinterpret_cast<Header*>(&m_Buffer[headerStart]);
            void*   obj    = &m_Buffer[header->s_ObjStart];

            header->s_Destructor(obj);
            m_Top = header->s_PrevTop;
        }
    }

    /** Destructs all live objects and resets the allocator to its initial empty state. */
    void Reset() noexcept { FreeToMarker(0); }

    [[nodiscard]] size_type Used() const noexcept { return m_Top; }
    [[nodiscard]] size_type Remaining() const noexcept { return N - m_Top; }
    [[nodiscard]] size_type Full() const noexcept { return m_Top == N; }
    [[nodiscard]] size_type Empty() const noexcept { return m_Top == 0; }
};

}
#include "LinearAllocator.hpp"

using namespace asge::mem;

std::size_t asge::mem::_internal::AlignUp(
    std::size_t inValue, std::size_t inAlignment
) noexcept {
    assert( (inAlignment & ( inAlignment - 1 )) == 0 && "Alignment must be a power of 2" );
    return ( inValue + inAlignment - 1 ) & ~( inAlignment - 1 );
}

asge::mem::LinearAllocator::LinearAllocator(void *inBuffer, std::size_t inSize)
: m_Base( static_cast<std::byte*>( inBuffer ) )
, m_Size( inSize )
{
}

asge::mem::LinearAllocator::LinearAllocator(LinearAllocator &&inOther)
: m_Base( inOther.m_Base ), m_Size( inOther.m_Size )
, m_Offset( inOther.m_Offset )
{
    inOther.m_Base = nullptr;
    inOther.m_Size = 0;
    inOther.m_Offset = 0;
}

LinearAllocator &asge::mem::LinearAllocator::operator=(LinearAllocator &&inOther)
{
    if ( this != &inOther )
    {
        m_Base = inOther.m_Base;
        m_Size = inOther.m_Size;
        m_Offset = inOther.m_Offset;
        inOther.m_Base = nullptr;
        inOther.m_Size = 0;
        inOther.m_Offset = 0;
    }

    return *this;
}

void *asge::mem::LinearAllocator::Alloc(size_type inBytes, size_type inAlign) noexcept
{
    size_type aligned = _internal::AlignUp( m_Offset, inAlign );
    if ( aligned + inBytes > m_Size ) return nullptr; // Out of memory
    void *outPtr = m_Base + aligned;
    m_Offset = aligned + inBytes;
    return outPtr;
}

asge::mem::LinearAllocator::marker_type 
asge::mem::LinearAllocator::GetMarker() const noexcept
{
    return Marker{ m_Offset };
}

void asge::mem::LinearAllocator::Rewind(marker_type inMarker)
{
    assert( inMarker.s_Offset <= m_Offset && "Marker is from the future" );
    m_Offset = inMarker.s_Offset;
}

void asge::mem::LinearAllocator::Reset() noexcept
{
    m_Offset = 0;
}

LinearAllocator::size_type asge::mem::LinearAllocator::Used() const noexcept
{
    return m_Offset;
}

LinearAllocator::size_type asge::mem::LinearAllocator::Remaining() const noexcept
{
    return m_Size - m_Offset;
}

LinearAllocator::size_type asge::mem::LinearAllocator::Capacity() const noexcept
{
    return m_Size;
}

bool asge::mem::LinearAllocator::Empty() const noexcept
{
    return m_Offset == 0;
}

asge::mem::ArenaAllocator::ArenaAllocator(size_type inSize)
    : m_Storage( std::make_unique<std::byte[]>( inSize ) )
{
    *static_cast<LinearAllocator*>(this) = LinearAllocator(m_Storage.get(), inSize);
}
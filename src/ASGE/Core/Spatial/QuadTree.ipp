#pragma once

#include <system_error>

namespace asge::spatial
{
template <typename T>
inline QuadNode* QuadTree<T>::createNode(math::Rect const &inBounds, std::uint16_t inDepth) noexcept
{
    void* mem = m_Allocator.Alloc( sizeof( QuadNode ), alignof( QuadNode ) );
    return mem ? ::new ( mem ) QuadNode( inBounds, inDepth ) : nullptr;
}

template <typename T>
inline typename QuadTree<T>::Block* QuadTree<T>::acquireBlock() noexcept
{
    Block* block = nullptr;

    if ( m_FreeBlocks != nullptr )
    {
        block = static_cast<Block*>( m_FreeBlocks );
        m_FreeBlocks = block->m_Next;
    }
    else
    {
        void* mem = m_Allocator.Alloc( sizeof( Block ), alignof( Block ) );
        if ( mem == nullptr ) return nullptr;
        block = ::new ( mem ) Block;
    }

    block->m_Next = nullptr;
    block->m_Used = 0;
    return block;
}

template <typename T>
inline void QuadTree<T>::releaseBlock(QuadBlockHeader *inBlock) noexcept
{
    inBlock->m_Next = m_FreeBlocks;
    m_FreeBlocks = inBlock;
}

template <typename T>
inline bool QuadTree<T>::addElementToNode(QuadNode *inNode, T inElement, math::Rect const &inBounds) noexcept
{
    Block* tail = static_cast<Block*>( inNode->m_Tail );

    if ( tail == nullptr || tail->m_Used == s_BlockCapacity )
    {
        Block* fresh = acquireBlock();
        if ( fresh == nullptr ) return false;

        if ( tail != nullptr ) tail->m_Next = fresh;
        else inNode->m_Head = fresh;

        inNode->m_Tail = fresh;
        tail = fresh;
    }

    tail->m_Bounds[tail->m_Used] = inBounds;
    tail->m_Values[tail->m_Used] = inElement;
    tail->m_Used++;
    inNode->m_Count++;
    return true;
}

template <typename T>
inline int QuadTree<T>::getQuadrantIndex(QuadNode const *inNode, math::Rect const &inBounds) const noexcept
{
    float const mid_x = inNode->m_Bounds.m_X + .5f * inNode->m_Bounds.m_Width;
    float const mid_y = inNode->m_Bounds.m_Y + .5f * inNode->m_Bounds.m_Height;

    bool const in_top       = ( inBounds.m_Y + inBounds.m_Height ) <= mid_y;
    bool const in_bottom    = inBounds.m_Y >= mid_y;
    bool const in_left      = ( inBounds.m_X + inBounds.m_Width ) <= mid_x;
    bool const in_right     = inBounds.m_X >= mid_x;

    if ( in_top    && in_left  ) return 0;
    if ( in_top    && in_right ) return 1;
    if ( in_bottom && in_left  ) return 2;
    if ( in_bottom && in_right ) return 3;

    return -1;
}

template <typename T>
inline bool QuadTree<T>::insert(QuadNode *inNode, T inElement, math::Rect const &inBounds) noexcept
{
    // A full leaf below the depth cap splits first; the element is then placed in the new layout
    if (   inNode->IsLeaf() && inNode->m_Count >= QuadNode::s_MaxNofElements
        && inNode->m_Depth < QuadNode::s_MaxDepth )
    {
        subdivide( inNode );
    }

    // Descend while the element fits entirely inside a single quadrant
    if ( !inNode->IsLeaf() )
    {
        if ( auto const quadrantIdx = getQuadrantIndex( inNode, inBounds ); quadrantIdx != -1 )
        {
            if ( insert( inNode->m_Children[quadrantIdx], inElement, inBounds ) ) return true;
        }
    }

    // Leaf, straddling a split line, or the child had no room: it belongs to this node
    return addElementToNode( inNode, inElement, inBounds );
}

template <typename T>
inline void QuadTree<T>::subdivide(QuadNode *inNode) noexcept
{
    float const half_w = inNode->m_Bounds.m_Width * .5f;
    float const half_h = inNode->m_Bounds.m_Height * .5f;
    float const x = inNode->m_Bounds.m_X;
    float const y = inNode->m_Bounds.m_Y;
    std::uint16_t const depth = inNode->m_Depth + 1;

    std::array<QuadNode*, 4> const children {
        createNode( math::Rect{ x,          y,          half_w, half_h }, depth ),
        createNode( math::Rect{ x + half_w, y,          half_w, half_h }, depth ),
        createNode( math::Rect{ x,          y + half_h, half_w, half_h }, depth ),
        createNode( math::Rect{ x + half_w, y + half_h, half_w, half_h }, depth )
    };

    // Arena exhausted: leave the node as an (overfull) leaf rather than half-split
    for ( QuadNode const* child : children )
    {
        if ( child == nullptr ) return;
    }

    inNode->m_Children = children;

    // Walk the node's elements, copying those that fit a quadrant into the child and
    // compacting the rest towards the front of the same chain. The write cursor never
    // passes the read cursor, so this is safe in place.
    QuadBlockHeader* writeBlock = inNode->m_Head;
    std::uint32_t    writeIdx   = 0;
    std::uint32_t    kept       = 0;

    for ( QuadBlockHeader* h = inNode->m_Head; h != nullptr; h = h->m_Next )
    {
        Block* readBlock = static_cast<Block*>( h );

        for ( std::uint32_t i = 0; i < readBlock->m_Used; ++i )
        {
            math::Rect const bounds = readBlock->m_Bounds[i];
            T const          value  = readBlock->m_Values[i];

            int const quadrantIdx = getQuadrantIndex( inNode, bounds );
            if ( quadrantIdx != -1 && addElementToNode( inNode->m_Children[quadrantIdx], value, bounds ) )
            {
                continue;
            }

            Block* target = static_cast<Block*>( writeBlock );
            target->m_Bounds[writeIdx] = bounds;
            target->m_Values[writeIdx] = value;
            ++kept;

            if ( ++writeIdx == s_BlockCapacity )
            {
                writeBlock = target->m_Next;
                writeIdx   = 0;
            }
        }
    }

    // Keep only the blocks the surviving elements need and recycle the rest
    std::uint32_t const keptBlocks = ( kept + s_BlockCapacity - 1 ) / s_BlockCapacity;
    QuadBlockHeader* last = nullptr;
    QuadBlockHeader* cur  = inNode->m_Head;

    for ( std::uint32_t b = 0; b < keptBlocks; ++b )
    {
        last = cur;
        cur  = cur->m_Next;
    }

    while ( cur != nullptr )
    {
        QuadBlockHeader* const next = cur->m_Next;
        releaseBlock( cur );
        cur = next;
    }

    if ( last != nullptr )
    {
        last->m_Used = kept - ( keptBlocks - 1 ) * static_cast<std::uint32_t>( s_BlockCapacity );
        last->m_Next = nullptr;
    }

    inNode->m_Head  = ( keptBlocks != 0 ) ? inNode->m_Head : nullptr;
    inNode->m_Tail  = last;
    inNode->m_Count = kept;
}

template <typename T>
template <typename Visitor>
inline bool QuadTree<T>::query(QuadNode const *inNode, math::Rect const &inRect, Visitor &inVisitor) const
{
    if ( !math::AabbOverlap( inNode->m_Bounds, inRect ) ) return true;
    
    for ( QuadBlockHeader const* h = inNode->m_Head; h != nullptr; h = h->m_Next )
    {
        Block const* block = static_cast<Block const*>( h );

        for ( std::uint32_t i = 0; i < block->m_Used; ++i )
        {
            if ( math::AabbOverlap( block->m_Bounds[i], inRect ) && !inVisitor( block->m_Values[i] ) )
            {
                return false;
            }
        }
    }

    if ( !inNode->IsLeaf() )
    {
        for ( QuadNode const* child : inNode->m_Children )
        {
            if ( !query( child, inRect, inVisitor ) )
            {
                return false;
            }
        }
    }

    return true;
}

template <typename T>
inline QuadTree<T>::QuadTree(std::size_t inArenaBytes, std::size_t inMaxElements)
    : m_Allocator( inArenaBytes ), m_MaxElements( inMaxElements )
{
}

template <typename T>
inline void QuadTree<T>::Reset(float inW, float inH) noexcept
{
    m_Allocator.Reset();
    m_FreeBlocks = nullptr;
    m_Size = 0;
    m_Root = createNode( math::Rect{ 0.0f, 0.0f, inW, inH }, 0 );
}

template <typename T>
inline asge::BoolResult QuadTree<T>::Insert(T inElement, math::Rect const &inBounds) noexcept
{
    if ( m_Root == nullptr )
    {
        return BoolResult::Err( std::make_error_code( std::errc::operation_not_permitted ), "tree not initialized: call Reset first" );
    }

    if ( !math::Contains( m_Root->m_Bounds, inBounds ) )
    {
        return BoolResult::Err( std::make_error_code( std::errc::invalid_argument ), "out of bounds" );
    }

    if ( m_Size >= m_MaxElements )
    {
        return BoolResult::Err( std::make_error_code( std::errc::no_buffer_space ), "element capacity reached" );
    }

    if ( !insert( m_Root, inElement, inBounds ) )
    {
        return BoolResult::Err( std::make_error_code( std::errc::no_buffer_space ), "arena out of memory" );
    }

    ++m_Size;
    return BoolResult::Ok();
}

template <typename T>
inline std::size_t QuadTree<T>::Size() const noexcept
{
    return m_Size;
}

template <typename T>
template <typename Visitor>
inline void QuadTree<T>::Query(math::Rect const &inRect, Visitor &&inVisitor) const
{
    if ( m_Root ) query( m_Root, inRect, inVisitor );
}

}

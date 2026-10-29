#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <system_error>
#include "QuadTree.hpp"

namespace asge::spatial
{
template <typename T>
inline QuadNode* QuadTree<T>::createNode(math::Rect const &inBounds, std::uint16_t inDepth) noexcept
{
    void* mem = m_Allocator.Alloc( sizeof( QuadNode ), alignof( QuadNode ) );
    if ( mem == nullptr )
    {
        m_Exhausted = true;
        return nullptr;
    }

    ++m_NodeCount;
    m_MaxDepth = std::max( m_MaxDepth, inDepth );
    return ::new ( mem ) QuadNode( inBounds, inDepth );
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
        if ( mem == nullptr )
        {
            m_Exhausted = true;
            return nullptr;
        }
        block = ::new ( mem ) Block;
    }

    block->m_Next = nullptr;
    block->m_Used = 0;
    return block;
}

template <typename T>
inline void QuadTree<T>::releaseBlock(QuadBlockHeader *inBlock) noexcept
{
#ifndef NDEBUG
    // O(free blocks) walk, debug builds only: catches a block released twice
    for ( QuadBlockHeader* it = m_FreeBlocks; it != nullptr; it = it->m_Next )
    {
        ASGE_ASSERT( it != inBlock, "QuadTree block released twice" );
    }
#endif

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
template <typename NodePred, typename ElemPred, typename Visitor>
inline bool QuadTree<T>::traverse(QuadNode const *inNode, NodePred inNodePred, ElemPred inElemPred, Visitor &inVisitor) const
{
    if ( !inNodePred( inNode->m_Bounds ) ) return true;

    std::array<QuadNode const*, s_QueryStackSize> stack;
    std::size_t top = 0;
    stack[top++] = inNode;

    while ( top != 0 )
    {
        QuadNode const* node = stack[--top];

        for ( QuadBlockHeader const* h = node->m_Head; h != nullptr; h = h->m_Next )
        {
            Block const* block = static_cast<Block const*>( h );

            for ( std::uint32_t i = 0; i < block->m_Used; ++i )
            {
                if ( inElemPred( block->m_Bounds[i] ) && !inVisitor( block->m_Values[i] ) )
                {
                    return false;
                }
            }
        }

        if ( node->IsLeaf() ) continue;

        // Pushed in reverse so child 0 is popped first
        for ( int c = 3; c >= 0; --c )
        {
            if ( inNodePred( node->m_Children[c]->m_Bounds ) )
            {
                stack[top++] = node->m_Children[c];
            }
        }
    }

    return true;
}

template <typename T>
inline void QuadTree<T>::nearest(QuadNode const *inNode, math::Float2 const &inPoint,
                                 float &ioBestDistance, std::optional<T> &ioBest) const noexcept
{
    for ( QuadBlockHeader const* h = inNode->m_Head; h != nullptr; h = h->m_Next )
    {
        Block const* block = static_cast<Block const*>( h );

        for ( std::uint32_t i = 0; i < block->m_Used; ++i )
        {
            float const d = math::Distance( block->m_Bounds[i], inPoint );
            if ( d < ioBestDistance )
            {
                ioBestDistance = d;
                ioBest = block->m_Values[i];
            }
        }
    }

    if ( inNode->IsLeaf() ) return;

    // Visit the closest quadrants first so the bound tightens early and prunes the rest
    std::array<std::pair<float, QuadNode const*>, 4> order;
    for ( int c = 0; c < 4; ++c )
    {
        order[c] = { math::Distance( inNode->m_Children[c]->m_Bounds, inPoint ), inNode->m_Children[c] };
    }
    std::sort( order.begin(), order.end(), []( auto const& a, auto const& b ) { return a.first < b.first; } );

    for ( auto const& [distance, child] : order )
    {
        if ( distance < ioBestDistance ) nearest( child, inPoint, ioBestDistance, ioBest );
    }
}

template <typename T>
template <typename Visitor>
inline bool QuadTree<T>::visitPairs(
    QuadNode const *inNode, std::vector<Entry> &ioScratch, 
    std::size_t inAncBegin, std::size_t inAncEnd, Visitor &inVisitor) const
{
    // Pairs inside the node, and between the node's elements and the ancestor elements
    for ( QuadBlockHeader const* h1 = inNode->m_Head; h1 != nullptr; h1 = h1->m_Next )
    {
        Block const* block1 = static_cast<Block const*>( h1 );

        for ( std::uint32_t i = 0; i < block1->m_Used; ++i )
        {
            math::Rect const& rect  = block1->m_Bounds[i];
            T const&          value = block1->m_Values[i];

            for ( std::size_t a = inAncBegin; a < inAncEnd; ++a )
            {
                if ( math::AabbOverlap( rect, ioScratch[a].m_Bounds ) && !inVisitor( value, ioScratch[a].m_Value ) )
                {
                    return false;
                }
            }

            for ( QuadBlockHeader const* h2 = h1; h2 != nullptr; h2 = h2->m_Next )
            {
                Block const* block2 = static_cast<Block const*>( h2 );

                for ( std::uint32_t j = ( h2 == h1 ) ? i + 1 : 0; j < block2->m_Used; ++j )
                {
                    if ( math::AabbOverlap( rect, block2->m_Bounds[j] ) && !inVisitor( value, block2->m_Values[j] ) )
                    {
                        return false;
                    }
                }
            }
        }
    }

    if ( inNode->IsLeaf() ) return true;

    for ( QuadNode const* child : inNode->m_Children )
    {
        // Only what overlaps the child's area can pair with something inside it
        std::size_t const childBegin = ioScratch.size();

        for ( std::size_t a = inAncBegin; a < inAncEnd; ++a )
        {
            if ( math::AabbOverlap( ioScratch[a].m_Bounds, child->m_Bounds ) )
            {
                Entry const entry = ioScratch[a];
                ioScratch.push_back( entry );
            }
        }

        for ( QuadBlockHeader const* h = inNode->m_Head; h != nullptr; h = h->m_Next )
        {
            Block const* block = static_cast<Block const*>( h );

            for ( std::uint32_t i = 0; i < block->m_Used; ++i )
            {
                if ( math::AabbOverlap( block->m_Bounds[i], child->m_Bounds ) )
                {
                    ioScratch.push_back( Entry{ block->m_Bounds[i], block->m_Values[i] } );
                }
            }
        }

        bool const keepGoing = visitPairs( child, ioScratch, childBegin, ioScratch.size(), inVisitor );
        ioScratch.erase( ioScratch.begin() + static_cast<std::ptrdiff_t>( childBegin ), ioScratch.end() );

        if ( !keepGoing ) return false;
    }

    return true;
}

template <typename T>
inline QuadTree<T>::QuadTree(std::size_t inArenaBytes, std::size_t inMaxElements)
    : m_Allocator( inArenaBytes ), m_MaxElements( inMaxElements )
{
}

template <typename T>
inline asge::BoolResult QuadTree<T>::Reset(float inW, float inH) noexcept
{
    return Reset( math::Rect{ 0.0f, 0.0f, inW, inH } );
}

template <typename T>
inline asge::BoolResult QuadTree<T>::Reset(math::Rect const &inBounds) noexcept
{
    m_Allocator.Reset();
    m_FreeBlocks = nullptr;
    m_Size = 0;
    m_NodeCount = 0;
    m_MaxDepth = 0;
    m_Exhausted = false;
    m_Root = nullptr;

    if ( !( std::isfinite( inBounds.m_X ) && std::isfinite( inBounds.m_Y ) &&
            std::isfinite( inBounds.m_Width ) && std::isfinite( inBounds.m_Height ) &&
            inBounds.m_Width > 0.0f && inBounds.m_Height > 0.0f ) )
    {
        return BoolResult::Err( std::make_error_code( std::errc::invalid_argument ), "tree bounds must be finite with positive size" );
    }

    m_Root = createNode( inBounds, 0 );
    if ( m_Root == nullptr )
    {
        return BoolResult::Err( std::make_error_code( std::errc::no_buffer_space ), "arena too small for the root node" );
    }

    return BoolResult::Ok();
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
    if ( m_Root == nullptr ) return;

    auto const overlaps = [&inRect]( math::Rect const& inBounds ) { return math::AabbOverlap( inBounds, inRect ); };
    traverse( m_Root, overlaps, overlaps, inVisitor );
}

template <typename T>
template <typename Visitor>
inline void QuadTree<T>::Query(math::Float2 const &inPoint, Visitor &&inVisitor) const
{
    if ( m_Root == nullptr ) return;

    auto const contains = [&inPoint]( math::Rect const& inBounds ) { return math::Contains( inBounds, inPoint ); };
    traverse( m_Root, contains, contains, inVisitor );
}

template <typename T>
template <typename Visitor>
inline void QuadTree<T>::Query(math::Circle const &inCircle, Visitor &&inVisitor) const
{
    if ( m_Root == nullptr ) return;

    auto const reaches = [&inCircle]( math::Rect const& inBounds )
    {
        return math::Distance( inBounds, inCircle.m_Center ) < inCircle.m_Radius;
    };
    traverse( m_Root, reaches, reaches, inVisitor );
}

template <typename T>
inline std::optional<T> QuadTree<T>::Nearest(math::Float2 const &inPoint) const noexcept
{
    std::optional<T> best;
    if ( m_Root == nullptr ) return best;

    float bestDistance = std::numeric_limits<float>::infinity();
    nearest( m_Root, inPoint, bestDistance, best );
    return best;
}

template <typename T>
inline std::size_t QuadTree<T>::NodeCount() const noexcept
{
    return m_NodeCount;
}

template <typename T>
inline std::uint16_t QuadTree<T>::MaxDepthReached() const noexcept
{
    return m_MaxDepth;
}

template <typename T>
inline bool QuadTree<T>::ArenaExhausted() const noexcept
{
    return m_Exhausted;
}

template <typename T>
inline std::size_t QuadTree<T>::ArenaUsed() const noexcept
{
    return m_Allocator.Used();
}

template <typename T>
inline std::size_t QuadTree<T>::ArenaCapacity() const noexcept
{
    return m_Allocator.Capacity();
}

template <typename T>
template <typename Visitor>
inline void QuadTree<T>::ForEachPair(Visitor &&inVisitor) const
{
    if ( m_Root == nullptr ) return;

    std::vector<Entry> scratch;
    scratch.reserve( 256 );
    visitPairs( m_Root, scratch, 0, 0, inVisitor );
}

}

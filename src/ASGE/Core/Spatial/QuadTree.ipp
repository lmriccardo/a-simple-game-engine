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
inline void QuadTree<T>::addElementToNode(QuadNode *inNode, T inElement, math::Rect const &inBounds) noexcept
{
    std::uint32_t idx = static_cast<std::uint32_t>( m_Elements.size() );

    m_Elements.push_back( Element{ 
        .m_Value = inElement, .m_Next = inNode->m_FirstElement, .m_Bounds = inBounds 
    } );

    inNode->m_FirstElement = idx;
    inNode->m_Count++;
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
inline void QuadTree<T>::insert(QuadNode *inNode, T inElement, math::Rect const &inBounds) noexcept
{
    // Descend while the element fits entirely inside a single quadrant
    if ( !inNode->IsLeaf() )
    {
        if ( auto const quadrantIdx = getQuadrantIndex( inNode, inBounds ); quadrantIdx != -1 )
        {
            insert( inNode->m_Children[quadrantIdx], inElement, inBounds );
            return;
        }

        // Straddles a split line: it belongs to this node
        addElementToNode( inNode, inElement, inBounds );
        return;
    }

    addElementToNode( inNode, inElement, inBounds );

    if ( inNode->m_Count > QuadNode::s_MaxNofElements && inNode->m_Depth < QuadNode::s_MaxDepth )
    {
        subdivide( inNode );
    }
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

    // Redistribute the node's elements: those fitting a quadrant move down
    auto currentElementIdx = inNode->m_FirstElement;
    inNode->m_FirstElement = k_NoNext;
    inNode->m_Count = 0;

    while ( currentElementIdx != k_NoNext )
    {
        Element& currElement = m_Elements[currentElementIdx];
        auto const nextIdx = currElement.m_Next;
        int const targetChild = getQuadrantIndex( inNode, currElement.m_Bounds );

        QuadNode* targetNode = ( targetChild != -1 ) ? inNode->m_Children[targetChild] : inNode;
        currElement.m_Next = targetNode->m_FirstElement;
        targetNode->m_FirstElement = currentElementIdx;
        targetNode->m_Count++;

        currentElementIdx = nextIdx;
    }
}

template <typename T>
template <typename Visitor>
inline bool QuadTree<T>::query(QuadNode const *inNode, math::Rect const &inRect, Visitor &inVisitor) const
{
    if ( !math::AabbOverlap( inNode->m_Bounds, inRect ) ) return true;
    
    for ( auto i = inNode->m_FirstElement; i != k_NoNext; i = m_Elements[i].m_Next )
    {
        if ( math::AabbOverlap( m_Elements[i].m_Bounds, inRect ) )
        {
            if ( !inVisitor( m_Elements[i].m_Value ) )
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
inline QuadTree<T>::QuadTree(std::size_t inMaxNodeBytes, std::size_t inMaxElements)
    : m_Allocator( inMaxNodeBytes ), m_MaxElements( inMaxElements )
{
    m_Elements.reserve( inMaxElements );
}

template <typename T>
inline void QuadTree<T>::Reset(float inW, float inH) noexcept
{
    m_Allocator.Reset();
    m_Elements.clear();
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

    if ( m_Elements.size() >= m_MaxElements )
    {
        return BoolResult::Err( std::make_error_code( std::errc::no_buffer_space ), "element capacity reached" );
    }

    insert( m_Root, inElement, inBounds );
    return BoolResult::Ok();
}

template <typename T>
inline std::size_t QuadTree<T>::Size() const noexcept
{
    return m_Elements.size();
}

template <typename T>
template <typename Visitor>
inline void QuadTree<T>::Query(math::Rect const &inRect, Visitor &&inVisitor) const
{
    if ( m_Root ) query( m_Root, inRect, inVisitor );
}

}

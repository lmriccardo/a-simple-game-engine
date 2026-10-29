#include "QuadTree.hpp"

asge::spatial::QuadNode::QuadNode( math::Rect const& inBounds, std::uint16_t inDepth )
    : m_Bounds( inBounds ), m_Depth( inDepth )
{
    m_Children.fill( nullptr );
}

bool asge::spatial::QuadNode::IsLeaf() const noexcept
{
    return m_Children[0] == nullptr;
}

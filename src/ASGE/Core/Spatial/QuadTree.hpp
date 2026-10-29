#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Collision.hpp>
#include <ASGE/Core/Memory/LinearAllocator.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::spatial
{

inline static constexpr std::uint32_t k_NoNext = UINT32_MAX;
 
struct QuadNode
{
    static constexpr std::uint16_t s_MaxNofElements = 8;
    static constexpr std::uint16_t s_MaxDepth       = 8;

    math::Rect                  m_Bounds; // The bounds of the current quadtree node
    std::array<QuadNode*, 4>    m_Children; // The 4 quadrant subdivision

    std::uint32_t m_FirstElement    { k_NoNext }; // First element pointed to by the node
    std::uint16_t m_Count           { 0 };        // Total number of elements in the node
    std::uint16_t m_Depth           { 0 };        // Distance from the root (root is 0)
    
    QuadNode( math::Rect const& inBounds, std::uint16_t inDepth = 0 );
    QuadNode( QuadNode const& ) = delete;
    QuadNode& operator=( QuadNode const& ) = delete;

    /** @brief True if the node has not been subdivided into children. */
    bool IsLeaf() const noexcept;
};

/**
 * @brief Region quadtree over axis-aligned rectangles, rebuilt from scratch via Reset().
 *
 * Nodes live in a fixed-size arena and elements in a single pre-reserved vector, so
 * Insert never allocates; it fails with an error when either budget is exhausted.
 * An element sits in the deepest node whose quadrant fully contains it, so elements
 * straddling a split line stay in the parent. Elements cannot be removed individually.
 */
template<typename T>
class QuadTree
{
public:
    struct Element
    {
        T               m_Value;    // The value of the element
        std::uint32_t   m_Next;     // Next value pointed to 
        math::Rect      m_Bounds;   // Element bounds
    };

private:
    mem::ArenaAllocator     m_Allocator;        // Arena allocator for creating nodes
    std::vector<Element>    m_Elements;         // Linked list of node elements
    QuadNode*               m_Root { nullptr }; // Root Node of the Tree
    std::size_t             m_MaxElements;      // Element budget fixed at construction

    /** @brief Allocates a node from the arena, or returns nullptr if the arena is full. */
    QuadNode* createNode( math::Rect const& inBounds, std::uint16_t inDepth ) noexcept;

    /** @brief Pushes the element on the front of the node's element list. */
    void addElementToNode( QuadNode* inNode, T inElement, math::Rect const& inBounds ) noexcept;

    /** @brief Returns 0..3 if the input rect fits one quadrant, -1 otherwise */
    int getQuadrantIndex( QuadNode const* inNode, math::Rect const& inBounds ) const noexcept;

    /** @brief Recursively insert an element into the quad tree */
    void insert( QuadNode* inNode, T inElement, math::Rect const& inBounds ) noexcept;

    /** @brief Subdivide the input quad node into 4 different regions and reorganize elements */
    void subdivide( QuadNode* inNode ) noexcept;

    /** @brief Recursive query step; returns false once the visitor asked to stop. */
    template<typename Visitor>
    bool query( QuadNode const* inNode, math::Rect const& inRect, Visitor& inVisitor ) const;

public:
    /** @brief Reserves an arena of inMaxNodeBytes and room for inMaxElements elements. */
    QuadTree( std::size_t inMaxNodeBytes, std::size_t inMaxElements );

    /** @brief Drops every element and node, then starts a new empty tree covering [0,inW]x[0,inH]. */
    void Reset( float inW, float inH ) noexcept;

    /**
     * @brief Inserts an element with the given bounds.
     *
     * Errors if Reset() was never called, the bounds are not fully inside the root,
     * or the element budget is exhausted. Running out of node memory is not an error:
     * the node just stops subdividing.
     */
    BoolResult Insert( T inElement, math::Rect const& inBounds ) noexcept;

    /** @brief Number of elements currently stored. */
    std::size_t Size() const noexcept;

    /**
     * @brief Calls inVisitor(value) for every element overlapping inRect.
     *
     * The visitor returns true to continue or false to stop the traversal early.
     * Overlap is strict (AabbOverlap), so rects that only touch are not reported.
     */
    template<typename Visitor>
    void Query( math::Rect const& inRect, Visitor&& inVisitor ) const;
};

}

#include "QuadTree.ipp"

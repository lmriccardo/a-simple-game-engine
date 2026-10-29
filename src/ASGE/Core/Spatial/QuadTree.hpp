#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <vector>

#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/Geometry/Collision.hpp>
#include <ASGE/Core/Memory/LinearAllocator.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::spatial
{

/** @brief Type-independent head of an element block, so QuadNode can chain blocks without knowing T. */
struct QuadBlockHeader
{
    QuadBlockHeader*    m_Next { nullptr }; // Next block of the same node, or the free-list successor
    std::uint32_t       m_Used { 0 };       // Slots filled in this block; only a node's last block is partial
};

struct QuadNode
{
    static constexpr std::uint16_t s_MaxNofElements = 8;
    static constexpr std::uint16_t s_MaxDepth       = 8;

    math::Rect                  m_Bounds; // The bounds of the current quadtree node
    std::array<QuadNode*, 4>    m_Children; // The 4 quadrant subdivision

    QuadBlockHeader*    m_Head  { nullptr };    // First element block of the node
    QuadBlockHeader*    m_Tail  { nullptr };    // Last element block, the one new elements are appended to
    std::uint32_t       m_Count { 0 };          // Total number of elements in the node
    std::uint16_t       m_Depth { 0 };          // Distance from the root (root is 0)
    
    QuadNode( math::Rect const& inBounds, std::uint16_t inDepth = 0 );
    QuadNode( QuadNode const& ) = delete;
    QuadNode& operator=( QuadNode const& ) = delete;

    /** @brief True if the node has not been subdivided into children. */
    bool IsLeaf() const noexcept;
};

/**
 * @brief Region quadtree over axis-aligned rectangles, rebuilt from scratch via Reset().
 *
 * Nodes and the blocks holding their elements live in a fixed-size arena, so Insert never
 * allocates from the heap. A node's elements sit in contiguous 8-slot blocks (bounds and
 * values in separate arrays) chained together when the node overflows. An element sits in
 * the deepest node whose quadrant fully contains it, so elements straddling a split line
 * stay in the parent. Elements cannot be removed individually.
 */
template<typename T>
class QuadTree
{
    static_assert( std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>,
        "QuadTree values live in arena memory that is never destroyed" );

    static constexpr std::size_t s_BlockCapacity = QuadNode::s_MaxNofElements;

    // Popping a node pushes at most 4 children, so a walk of depth D never holds more than 3*D + 4 nodes
    static constexpr std::size_t s_QueryStackSize = 3 * QuadNode::s_MaxDepth + 4;

    /** @brief Fixed-size run of elements of one node, stored as parallel arrays. */
    struct Block : QuadBlockHeader
    {
        std::array<math::Rect, s_BlockCapacity>  m_Bounds; // Element bounds
        std::array<T, s_BlockCapacity>           m_Values; // Element values
    };

    /** @brief Copy of an element of a node above the one being visited, used by ForEachPair. */
    struct Entry
    {
        math::Rect  m_Bounds; // Element bounds
        T           m_Value;  // Element value
    };

    mem::ArenaAllocator m_Allocator;                    // Arena allocator for nodes and blocks
    QuadNode*           m_Root       { nullptr };       // Root Node of the Tree
    QuadBlockHeader*    m_FreeBlocks { nullptr };       // Blocks released by subdivision, reused first
    std::size_t         m_MaxElements;                  // Element budget fixed at construction
    std::size_t         m_Size       { 0 };             // Number of elements currently stored
    std::size_t         m_NodeCount  { 0 };             // Nodes created since the last Reset
    std::uint16_t       m_MaxDepth   { 0 };             // Deepest node created since the last Reset
    bool                m_Exhausted  { false };         // An arena allocation failed since the last Reset

    /** @brief Allocates a node from the arena, or returns nullptr if the arena is full. */
    QuadNode* createNode( math::Rect const& inBounds, std::uint16_t inDepth ) noexcept;

    /** @brief Returns an empty block from the free list or the arena, or nullptr if the arena is full. */
    Block* acquireBlock() noexcept;

    /** @brief Pushes the block on the free list for later reuse. */
    void releaseBlock( QuadBlockHeader* inBlock ) noexcept;

    /** @brief Appends the element to the node's last block, chaining a new one if full. False if the arena is full. */
    bool addElementToNode( QuadNode* inNode, T inElement, math::Rect const& inBounds ) noexcept;

    /** @brief Returns 0..3 if the input rect fits one quadrant, -1 otherwise */
    int getQuadrantIndex( QuadNode const* inNode, math::Rect const& inBounds ) const noexcept;

    /** @brief Recursively inserts an element into the tree. False only if the arena has no room for it. */
    bool insert( QuadNode* inNode, T inElement, math::Rect const& inBounds ) noexcept;

    /**
     * @brief Splits the leaf into 4 children and moves the elements that fit a quadrant down.
     *
     * Elements that straddle a split line, or whose child could not get a block, stay in
     * the node. If the arena cannot hold 4 children the node is left as a leaf.
     */
    void subdivide( QuadNode* inNode ) noexcept;

    /**
     * @brief Visits the elements of the subtree for which inElemPred holds, using an explicit fixed-size stack.
     *
     * A child is entered only if inNodePred holds for its bounds, so the node predicate must be
     * true whenever the element predicate is true for something inside the node. Nodes are visited
     * in the same order as a recursive walk (parent, then children 0..3). Returns false once the
     * visitor asked to stop.
     */
    template<typename NodePred, typename ElemPred, typename Visitor>
    bool traverse( QuadNode const* inNode, NodePred inNodePred, ElemPred inElemPred, Visitor& inVisitor ) const;

    /** @brief Branch-and-bound step of Nearest: updates the best element found so far in the subtree. */
    void nearest( QuadNode const* inNode, math::Float2 const& inPoint,
                  float& ioBestDistance, std::optional<T>& ioBest ) const noexcept;

    /**
     * @brief Reports the overlapping pairs found in the subtree of inNode.
     *
     * ioScratch[inAncBegin, inAncEnd) holds the ancestor elements that overlap inNode and is
     * the end of the buffer on entry; children append their own filtered list after it and
     * truncate back once visited. Returns false once the visitor asked to stop.
     */
    template<typename Visitor>
    bool visitPairs( QuadNode const* inNode, std::vector<Entry>& ioScratch,
                     std::size_t inAncBegin, std::size_t inAncEnd, Visitor& inVisitor ) const;

public:
    /** @brief Reserves an arena of inArenaBytes for nodes and element blocks, and caps the tree at inMaxElements. */
    QuadTree( std::size_t inArenaBytes, std::size_t inMaxElements );

    /**
     * @brief Drops every element and node, then starts a new empty tree covering [0,inW]x[0,inH].
     *
     * Errors with invalid_argument unless both sizes are finite and positive, and with
     * no_buffer_space if the arena cannot even hold the root. After an error the tree is
     * empty and uninitialised, so Insert fails until a Reset succeeds.
     */
    BoolResult Reset( float inW, float inH ) noexcept;

    /**
     * @brief Inserts an element with the given bounds.
     *
     * Errors if Reset() was never called, the bounds are not fully inside the root, the
     * element budget is exhausted, or the arena has no room left for the element. A failed
     * insert leaves the tree valid and unchanged in content.
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

    /**
     * @brief Calls inVisitor(value) for every element whose rect strictly contains inPoint.
     *
     * Same visitor protocol as the Rect overload; a point exactly on an element's edge is not inside it.
     */
    template<typename Visitor>
    void Query( math::Float2 const& inPoint, Visitor&& inVisitor ) const;

    /**
     * @brief Calls inVisitor(value) for every element whose rect is closer to the circle's
     *        center than its radius.
     *
     * Same visitor protocol as the Rect overload; an element that only touches the circle is not reported.
     */
    template<typename Visitor>
    void Query( math::Circle const& inCircle, Visitor&& inVisitor ) const;

    /**
     * @brief Returns the value of the element whose rect is closest to inPoint.
     *
     * The distance is 0 for a point inside or on an element. When several elements are equally
     * close, which one is returned is unspecified. Empty if the tree is empty or uninitialised.
     */
    std::optional<T> Nearest( math::Float2 const& inPoint ) const noexcept;

    /** @brief Number of nodes created since the last Reset, root included. */
    std::size_t NodeCount() const noexcept;

    /** @brief Depth of the deepest node created since the last Reset (the root is 0). */
    std::uint16_t MaxDepthReached() const noexcept;

    /**
     * @brief True if an arena allocation failed since the last Reset.
     *
     * It means the tree degraded: some leaf could not split, or an Insert was refused for lack
     * of memory. Compare ArenaUsed() with ArenaCapacity() to size the arena.
     */
    bool ArenaExhausted() const noexcept;

    /** @brief Arena bytes consumed so far by nodes and element blocks. */
    std::size_t ArenaUsed() const noexcept;

    /** @brief Total arena size in bytes. */
    std::size_t ArenaCapacity() const noexcept;

    /**
     * @brief Calls inVisitor(a, b) once for every unordered pair of overlapping elements.
     *
     * Never pairs an element with itself and never reports a pair twice; the order of the
     * pairs and of the two values within a pair is unspecified. The visitor returns true to
     * continue or false to stop early. Allocates one scratch buffer per call.
     */
    template<typename Visitor>
    void ForEachPair( Visitor&& inVisitor ) const;
};

}

#include "QuadTree.ipp"

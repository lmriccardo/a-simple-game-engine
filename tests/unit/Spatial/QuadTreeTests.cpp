#include <ASGE/Core/Spatial/QuadTree.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using namespace asge::spatial;
using asge::math::Rect;

constexpr std::size_t k_NodeBytes   = 64 * 1024;
constexpr std::size_t k_MaxElements = 1024;

class QuadTreeTest : public ::testing::Test
{
protected:
    QuadTree<int> m_Tree { k_NodeBytes, k_MaxElements };

    void SetUp() override { m_Tree.Reset( 100.0f, 100.0f ); }

    std::vector<int> Collect( Rect const& inRect ) const
    {
        std::vector<int> out;
        m_Tree.Query( inRect, [&out]( int v ) { out.push_back( v ); return true; } );
        std::sort( out.begin(), out.end() );
        return out;
    }
};

// ─── QuadNode ─────────────────────────────────────────────────────────

TEST(QuadNodeTest, NewNode_IsEmptyLeaf)
{
    QuadNode node( Rect{ 0.0f, 0.0f, 10.0f, 10.0f } );
    EXPECT_TRUE( node.IsLeaf() );
    EXPECT_EQ( node.m_Count, 0 );
    EXPECT_EQ( node.m_Head, nullptr );
    EXPECT_EQ( node.m_Tail, nullptr );
    EXPECT_EQ( node.m_Depth, 0 );
}

// ─── Insert ───────────────────────────────────────────────────────────

TEST_F(QuadTreeTest, Insert_BeforeReset_ReturnsNotPermittedError)
{
    QuadTree<int> tree( k_NodeBytes, k_MaxElements );
    auto result = tree.Insert( 1, Rect{ 0.0f, 0.0f, 1.0f, 1.0f } );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::operation_not_permitted ) );
}

TEST_F(QuadTreeTest, Insert_InsideRoot_Succeeds)
{
    EXPECT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    EXPECT_EQ( m_Tree.Size(), 1u );
}

TEST_F(QuadTreeTest, Insert_FlushWithRootEdges_Succeeds)
{
    EXPECT_TRUE( m_Tree.Insert( 1, Rect{ 0.0f, 0.0f, 100.0f, 100.0f } ).IsOk() );
}

TEST_F(QuadTreeTest, Insert_PartiallyOutsideRoot_ReturnsInvalidArgument)
{
    auto result = m_Tree.Insert( 1, Rect{ 95.0f, 95.0f, 10.0f, 10.0f } );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::invalid_argument ) );
    EXPECT_EQ( m_Tree.Size(), 0u );
}

TEST_F(QuadTreeTest, Insert_NegativeOrigin_ReturnsInvalidArgument)
{
    auto result = m_Tree.Insert( 1, Rect{ -1.0f, 0.0f, 5.0f, 5.0f } );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::invalid_argument ) );
}

TEST_F(QuadTreeTest, Insert_PastElementCapacity_ReturnsNoBufferSpace)
{
    QuadTree<int> tree( k_NodeBytes, 3 );
    tree.Reset( 100.0f, 100.0f );
    for ( int i = 0; i < 3; ++i )
    {
        ASSERT_TRUE( tree.Insert( i, Rect{ 1.0f, 1.0f, 1.0f, 1.0f } ).IsOk() );
    }

    auto result = tree.Insert( 3, Rect{ 1.0f, 1.0f, 1.0f, 1.0f } );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::no_buffer_space ) );
    EXPECT_EQ( tree.Size(), 3u );
}

// ─── Query ────────────────────────────────────────────────────────────

TEST_F(QuadTreeTest, Query_BeforeReset_VisitsNothing)
{
    QuadTree<int> tree( k_NodeBytes, k_MaxElements );
    int calls = 0;
    tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&calls]( int ) { ++calls; return true; } );
    EXPECT_EQ( calls, 0 );
}

TEST_F(QuadTreeTest, Query_EmptyTree_VisitsNothing)
{
    EXPECT_TRUE( Collect( Rect{ 0.0f, 0.0f, 100.0f, 100.0f } ).empty() );
}

TEST_F(QuadTreeTest, Query_ReturnsOnlyOverlappingElements)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 80.0f, 80.0f, 5.0f, 5.0f } ).IsOk() );

    EXPECT_EQ( Collect( Rect{ 0.0f, 0.0f, 20.0f, 20.0f } ), std::vector<int>{ 1 } );
    EXPECT_EQ( Collect( Rect{ 70.0f, 70.0f, 30.0f, 30.0f } ), std::vector<int>{ 2 } );
}

TEST_F(QuadTreeTest, Query_TouchingEdgeOnly_IsNotReported)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 10.0f, 10.0f } ).IsOk() );
    EXPECT_TRUE( Collect( Rect{ 20.0f, 10.0f, 10.0f, 10.0f } ).empty() );
}

TEST_F(QuadTreeTest, Query_ElementStraddlingMidline_IsFoundFromEitherSide)
{
    ASSERT_TRUE( m_Tree.Insert( 7, Rect{ 45.0f, 45.0f, 10.0f, 10.0f } ).IsOk() );
    EXPECT_EQ( Collect( Rect{ 0.0f, 0.0f, 46.0f, 46.0f } ), std::vector<int>{ 7 } );
    EXPECT_EQ( Collect( Rect{ 54.0f, 54.0f, 46.0f, 46.0f } ), std::vector<int>{ 7 } );
}

TEST_F(QuadTreeTest, Query_VisitorReturningFalse_StopsTraversal)
{
    for ( int i = 0; i < 5; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    }

    int calls = 0;
    m_Tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&calls]( int ) { ++calls; return false; } );
    EXPECT_EQ( calls, 1 );
}

TEST_F(QuadTreeTest, Query_VisitorStopsAcrossSubdividedNodes)
{
    for ( int i = 0; i < 40; ++i )
    {
        float const x = static_cast<float>( ( i % 10 ) * 10 );
        float const y = static_cast<float>( ( i / 10 ) * 20 );
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ x + 1.0f, y + 1.0f, 2.0f, 2.0f } ).IsOk() );
    }

    int calls = 0;
    m_Tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&calls]( int ) { return ++calls < 3; } );
    EXPECT_EQ( calls, 3 );
}

// ─── Subdivision ──────────────────────────────────────────────────────

TEST_F(QuadTreeTest, Insert_ManyElementsInOneCorner_NoneLostOrDuplicated)
{
    int const count = 100;
    for ( int i = 0; i < count; ++i )
    {
        float const x = static_cast<float>( i % 10 );
        float const y = static_cast<float>( i / 10 );
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ x, y, 0.5f, 0.5f } ).IsOk() );
    }

    auto const all = Collect( Rect{ 0.0f, 0.0f, 100.0f, 100.0f } );
    ASSERT_EQ( all.size(), static_cast<std::size_t>( count ) );
    for ( int i = 0; i < count; ++i ) EXPECT_EQ( all[i], i );
}

TEST_F(QuadTreeTest, Insert_ManyStraddlingElements_EachReportedOnce)
{
    for ( int i = 0; i < 30; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 40.0f, 40.0f, 20.0f, 20.0f } ).IsOk() );
    }

    EXPECT_EQ( Collect( Rect{ 0.0f, 0.0f, 100.0f, 100.0f } ).size(), 30u );
}

TEST_F(QuadTreeTest, Insert_IdenticalRects_TerminatesViaDepthCap)
{
    for ( int i = 0; i < 200; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 1.0f, 1.0f, 0.1f, 0.1f } ).IsOk() );
    }

    EXPECT_EQ( Collect( Rect{ 0.0f, 0.0f, 5.0f, 5.0f } ).size(), 200u );
}

TEST_F(QuadTreeTest, Insert_ManyStraddlersInRoot_ChainBlocksAndAreAllFound)
{
    for ( int i = 0; i < 1000; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 40.0f, 40.0f, 20.0f, 20.0f } ).IsOk() );
    }

    EXPECT_EQ( Collect( Rect{ 45.0f, 45.0f, 1.0f, 1.0f } ).size(), 1000u );
}

TEST_F(QuadTreeTest, Insert_ArenaExhausted_ReturnsNoBufferSpaceAndKeepsStoredElements)
{
    QuadTree<int> tree( 512, k_MaxElements );
    tree.Reset( 100.0f, 100.0f );

    int accepted = 0;
    std::error_code firstError;
    for ( int i = 0; i < 50; ++i )
    {
        auto result = tree.Insert( i, Rect{ 1.0f + i, 1.0f, 0.5f, 0.5f } );
        if ( result.IsOk() ) ++accepted;
        else if ( !firstError ) firstError = result.Code();
    }

    EXPECT_GT( accepted, 0 );
    EXPECT_LT( accepted, 50 );
    EXPECT_EQ( firstError, std::make_error_code( std::errc::no_buffer_space ) );
    EXPECT_EQ( tree.Size(), static_cast<std::size_t>( accepted ) );

    int found = 0;
    tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&found]( int ) { ++found; return true; } );
    EXPECT_EQ( found, accepted );
}

TEST_F(QuadTreeTest, Insert_AnyArenaSize_NeverLosesAnAcceptedElement)
{
    for ( std::size_t bytes = 128; bytes <= 8192; bytes += 96 )
    {
        QuadTree<int> tree( bytes, k_MaxElements );
        tree.Reset( 100.0f, 100.0f );

        std::vector<int> accepted;
        for ( int i = 0; i < 120; ++i )
        {
            float const x = static_cast<float>( ( i * 7 ) % 90 );
            float const y = static_cast<float>( ( i * 13 ) % 90 );
            float const size = ( i % 5 == 0 ) ? 30.0f : 1.0f;
            if ( tree.Insert( i, Rect{ x, y, size, size } ).IsOk() ) accepted.push_back( i );
        }

        std::vector<int> found;
        tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&found]( int v ) { found.push_back( v ); return true; } );
        std::sort( found.begin(), found.end() );

        EXPECT_EQ( found, accepted ) << "arena bytes: " << bytes;
    }
}

TEST_F(QuadTreeTest, Query_MatchesBruteForceOnRandomData)
{
    std::mt19937 rng( 1234 );
    std::uniform_real_distribution<float> pos( 0.0f, 90.0f ), size( 0.5f, 10.0f );

    std::vector<Rect> rects;
    for ( int i = 0; i < 500; ++i )
    {
        Rect r{ pos( rng ), pos( rng ), size( rng ), size( rng ) };
        rects.push_back( r );
        ASSERT_TRUE( m_Tree.Insert( i, r ).IsOk() );
    }

    for ( int q = 0; q < 50; ++q )
    {
        Rect const area{ pos( rng ), pos( rng ), size( rng ) * 3.0f, size( rng ) * 3.0f };

        std::vector<int> expected;
        for ( int i = 0; i < static_cast<int>( rects.size() ); ++i )
        {
            if ( asge::math::AabbOverlap( rects[i], area ) ) expected.push_back( i );
        }

        EXPECT_EQ( Collect( area ), expected );
    }
}

TEST_F(QuadTreeTest, Query_DeepestTree_NeverOverflowsTheStack)
{
    // Coincident tiny rects force subdivision down to the depth cap in one corner
    for ( int i = 0; i < 300; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 0.0f, 0.0f, 0.01f, 0.01f } ).IsOk() );
    }
    for ( int i = 0; i < 300; ++i )
    {
        float const x = static_cast<float>( i % 100 );
        ASSERT_TRUE( m_Tree.Insert( 1000 + i, Rect{ x, x, 0.01f, 0.01f } ).IsOk() );
    }

    EXPECT_EQ( Collect( Rect{ 0.0f, 0.0f, 100.0f, 100.0f } ).size(), 600u );
}

TEST_F(QuadTreeTest, Query_VisitsParentBeforeChildrenInQuadrantOrder)
{
    int const straddlers = QuadNode::s_MaxNofElements;
    for ( int i = 0; i < straddlers; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( 100 + i, Rect{ 40.0f, 40.0f, 20.0f, 20.0f } ).IsOk() );
    }
    ASSERT_TRUE( m_Tree.Insert( 3, Rect{ 80.0f, 80.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 10.0f, 80.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 80.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 0, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );

    std::vector<int> order;
    m_Tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&order]( int v ) { order.push_back( v ); return true; } );

    ASSERT_EQ( order.size(), static_cast<std::size_t>( straddlers ) + 4u );
    EXPECT_GE( order[0], 100 ); // the straddlers held by the root come first
    EXPECT_EQ( std::vector<int>( order.end() - 4, order.end() ), ( std::vector<int>{ 0, 1, 2, 3 } ) );
}

// ─── ForEachPair ──────────────────────────────────────────────────────

using PairList = std::vector<std::pair<int, int>>;

PairList BrutePairs( std::vector<Rect> const& inRects, std::vector<int> const& inIds )
{
    PairList out;
    for ( std::size_t i = 0; i < inRects.size(); ++i )
    {
        for ( std::size_t j = i + 1; j < inRects.size(); ++j )
        {
            if ( asge::math::AabbOverlap( inRects[i], inRects[j] ) )
            {
                out.emplace_back( std::min( inIds[i], inIds[j] ), std::max( inIds[i], inIds[j] ) );
            }
        }
    }
    std::sort( out.begin(), out.end() );
    return out;
}

PairList TreePairs( QuadTree<int> const& inTree )
{
    PairList out;
    inTree.ForEachPair( [&out]( int a, int b ) { out.emplace_back( std::min( a, b ), std::max( a, b ) ); return true; } );
    std::sort( out.begin(), out.end() );
    return out;
}

TEST_F(QuadTreeTest, ForEachPair_BeforeReset_VisitsNothing)
{
    QuadTree<int> tree( k_NodeBytes, k_MaxElements );
    int calls = 0;
    tree.ForEachPair( [&calls]( int, int ) { ++calls; return true; } );
    EXPECT_EQ( calls, 0 );
}

TEST_F(QuadTreeTest, ForEachPair_SingleElement_VisitsNothing)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    EXPECT_TRUE( TreePairs( m_Tree ).empty() );
}

TEST_F(QuadTreeTest, ForEachPair_TwoOverlappingElements_ReportedOnce)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 12.0f, 12.0f, 5.0f, 5.0f } ).IsOk() );
    EXPECT_EQ( TreePairs( m_Tree ), ( PairList{ { 1, 2 } } ) );
}

TEST_F(QuadTreeTest, ForEachPair_TouchingElements_NotReported)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 15.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    EXPECT_TRUE( TreePairs( m_Tree ).empty() );
}

TEST_F(QuadTreeTest, ForEachPair_CoincidentElements_EveryCombinationOnce)
{
    for ( int i = 0; i < 20; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 5.0f, 5.0f, 2.0f, 2.0f } ).IsOk() );
    }
    EXPECT_EQ( TreePairs( m_Tree ).size(), 190u );
}

TEST_F(QuadTreeTest, ForEachPair_OverlappingStraddlersInRoot_Reported)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 30.0f, 30.0f, 40.0f, 40.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 40.0f, 40.0f, 40.0f, 40.0f } ).IsOk() );
    EXPECT_EQ( TreePairs( m_Tree ), ( PairList{ { 1, 2 } } ) );
}

TEST_F(QuadTreeTest, ForEachPair_StraddlerAgainstDeepElements_Reported)
{
    for ( int i = 0; i < 40; ++i )
    {
        float const x = static_cast<float>( 1 + ( i % 8 ) * 3 );
        float const y = static_cast<float>( 1 + ( i / 8 ) * 3 );
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ x, y, 1.0f, 1.0f } ).IsOk() );
    }
    ASSERT_TRUE( m_Tree.Insert( 100, Rect{ 0.0f, 0.0f, 60.0f, 60.0f } ).IsOk() );

    PairList const pairs = TreePairs( m_Tree );
    EXPECT_EQ( pairs.size(), 40u );
    for ( auto const& [a, b] : pairs ) EXPECT_EQ( b, 100 );
}

TEST_F(QuadTreeTest, ForEachPair_VisitorReturningFalse_StopsTraversal)
{
    for ( int i = 0; i < 20; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 5.0f, 5.0f, 2.0f, 2.0f } ).IsOk() );
    }

    int calls = 0;
    m_Tree.ForEachPair( [&calls]( int, int ) { return ++calls < 3; } );
    EXPECT_EQ( calls, 3 );
}

TEST_F(QuadTreeTest, ForEachPair_MatchesBruteForceOnRandomData)
{
    std::mt19937 rng( 99 );
    std::uniform_real_distribution<float> pos( 0.0f, 90.0f ), size( 0.5f, 6.0f ), big( 20.0f, 40.0f );

    std::vector<Rect> rects;
    std::vector<int> ids;
    for ( int i = 0; i < 600; ++i )
    {
        float const w = ( i % 25 == 0 ) ? big( rng ) : size( rng );
        float const h = ( i % 25 == 0 ) ? big( rng ) : size( rng );
        Rect r{ pos( rng ), pos( rng ), w, h };
        r.m_X = std::min( r.m_X, 100.0f - w );
        r.m_Y = std::min( r.m_Y, 100.0f - h );
        rects.push_back( r );
        ids.push_back( i );
        ASSERT_TRUE( m_Tree.Insert( i, r ).IsOk() );
    }

    PairList const expected = BrutePairs( rects, ids );
    PairList const actual   = TreePairs( m_Tree );

    EXPECT_EQ( actual, expected );
    std::set<std::pair<int, int>> const unique( actual.begin(), actual.end() );
    EXPECT_EQ( unique.size(), actual.size() ) << "duplicate pair";
}

TEST_F(QuadTreeTest, ForEachPair_AnyArenaSize_MatchesBruteForceOnAcceptedElements)
{
    for ( std::size_t bytes = 128; bytes <= 8192; bytes += 160 )
    {
        QuadTree<int> tree( bytes, k_MaxElements );
        tree.Reset( 100.0f, 100.0f );

        std::vector<Rect> rects;
        std::vector<int> ids;
        for ( int i = 0; i < 120; ++i )
        {
            float const size = ( i % 5 == 0 ) ? 30.0f : 3.0f;
            Rect const r{ static_cast<float>( ( i * 7 ) % 70 ), static_cast<float>( ( i * 13 ) % 70 ), size, size };
            if ( tree.Insert( i, r ).IsOk() ) { rects.push_back( r ); ids.push_back( i ); }
        }

        EXPECT_EQ( TreePairs( tree ), BrutePairs( rects, ids ) ) << "arena bytes: " << bytes;
    }
}

// ─── Query (point / circle) ───────────────────────────────────────────

using asge::math::Circle;
using asge::math::Float2;

TEST_F(QuadTreeTest, QueryPoint_InsideElement_Reported)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 10.0f, 10.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 50.0f, 50.0f, 10.0f, 10.0f } ).IsOk() );

    std::vector<int> found;
    m_Tree.Query( Float2{ 15.0f, 15.0f }, [&found]( int v ) { found.push_back( v ); return true; } );
    EXPECT_EQ( found, std::vector<int>{ 1 } );
}

TEST_F(QuadTreeTest, QueryPoint_OnEdgeOrOutside_NotReported)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 10.0f, 10.0f } ).IsOk() );

    int calls = 0;
    auto const count = [&calls]( int ) { ++calls; return true; };
    m_Tree.Query( Float2{ 10.0f, 15.0f }, count );
    m_Tree.Query( Float2{ 30.0f, 30.0f }, count );
    EXPECT_EQ( calls, 0 );
}

TEST_F(QuadTreeTest, QueryPoint_BeforeReset_VisitsNothing)
{
    QuadTree<int> tree( k_NodeBytes, k_MaxElements );
    int calls = 0;
    tree.Query( Float2{ 1.0f, 1.0f }, [&calls]( int ) { ++calls; return true; } );
    EXPECT_EQ( calls, 0 );
}

TEST_F(QuadTreeTest, QueryPoint_OverlappingElements_AllReportedAndCanStop)
{
    for ( int i = 0; i < 20; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 40.0f, 40.0f, 20.0f, 20.0f } ).IsOk() );
    }

    int calls = 0;
    m_Tree.Query( Float2{ 50.0f, 50.0f }, [&calls]( int ) { ++calls; return true; } );
    EXPECT_EQ( calls, 20 );

    calls = 0;
    m_Tree.Query( Float2{ 50.0f, 50.0f }, [&calls]( int ) { return ++calls < 4; } );
    EXPECT_EQ( calls, 4 );
}

TEST_F(QuadTreeTest, QueryCircle_ReportsElementsWithinRadius)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 50.0f, 50.0f, 4.0f, 4.0f } ).IsOk() );  // center inside the rect
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 60.0f, 50.0f, 4.0f, 4.0f } ).IsOk() );  // 5 away from a center at 55
    ASSERT_TRUE( m_Tree.Insert( 3, Rect{ 90.0f, 90.0f, 4.0f, 4.0f } ).IsOk() );  // far away

    EXPECT_EQ( [&] { std::vector<int> v; m_Tree.Query( Circle{ Float2{ 52.0f, 52.0f }, 10.0f }, [&v]( int x ) { v.push_back( x ); return true; } ); std::sort( v.begin(), v.end() ); return v; }(),
               ( std::vector<int>{ 1, 2 } ) );
}

TEST_F(QuadTreeTest, QueryCircle_TouchingOnly_NotReported)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 60.0f, 50.0f, 4.0f, 4.0f } ).IsOk() );

    int calls = 0;
    m_Tree.Query( Circle{ Float2{ 50.0f, 52.0f }, 10.0f }, [&calls]( int ) { ++calls; return true; } );
    EXPECT_EQ( calls, 0 );
}

TEST_F(QuadTreeTest, QueryCircle_VisitorReturningFalse_StopsTraversal)
{
    for ( int i = 0; i < 30; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 10.0f + i, 10.0f, 1.0f, 1.0f } ).IsOk() );
    }

    int calls = 0;
    m_Tree.Query( Circle{ Float2{ 25.0f, 10.0f }, 50.0f }, [&calls]( int ) { return ++calls < 5; } );
    EXPECT_EQ( calls, 5 );
}

TEST_F(QuadTreeTest, QueryPointAndCircle_MatchBruteForceOnRandomData)
{
    std::mt19937 rng( 5 );
    std::uniform_real_distribution<float> pos( 0.0f, 90.0f ), size( 0.5f, 8.0f ), big( 20.0f, 40.0f );

    std::vector<Rect> rects;
    for ( int i = 0; i < 600; ++i )
    {
        float const w = ( i % 25 == 0 ) ? big( rng ) : size( rng );
        float const h = ( i % 25 == 0 ) ? big( rng ) : size( rng );
        Rect r{ pos( rng ), pos( rng ), w, h };
        r.m_X = std::min( r.m_X, 100.0f - w );
        r.m_Y = std::min( r.m_Y, 100.0f - h );
        rects.push_back( r );
        ASSERT_TRUE( m_Tree.Insert( i, r ).IsOk() );
    }

    for ( int q = 0; q < 100; ++q )
    {
        Float2 const point{ pos( rng ), pos( rng ) };
        float const radius = size( rng ) * 2.0f;

        std::vector<int> expectedPoint, expectedCircle, gotPoint, gotCircle;
        for ( int i = 0; i < static_cast<int>( rects.size() ); ++i )
        {
            if ( asge::math::Contains( rects[i], point ) ) expectedPoint.push_back( i );
            if ( asge::math::Distance( rects[i], point ) < radius ) expectedCircle.push_back( i );
        }

        m_Tree.Query( point, [&gotPoint]( int v ) { gotPoint.push_back( v ); return true; } );
        m_Tree.Query( Circle{ point, radius }, [&gotCircle]( int v ) { gotCircle.push_back( v ); return true; } );
        std::sort( gotPoint.begin(), gotPoint.end() );
        std::sort( gotCircle.begin(), gotCircle.end() );

        EXPECT_EQ( gotPoint, expectedPoint );
        EXPECT_EQ( gotCircle, expectedCircle );
    }
}

// ─── Nearest ──────────────────────────────────────────────────────────

TEST_F(QuadTreeTest, Nearest_BeforeResetOrWhenEmpty_ReturnsNothing)
{
    QuadTree<int> uninitialised( k_NodeBytes, k_MaxElements );
    EXPECT_FALSE( uninitialised.Nearest( Float2{ 1.0f, 1.0f } ).has_value() );
    EXPECT_FALSE( m_Tree.Nearest( Float2{ 1.0f, 1.0f } ).has_value() );
}

TEST_F(QuadTreeTest, Nearest_PicksTheClosestElement)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 80.0f, 80.0f, 5.0f, 5.0f } ).IsOk() );

    EXPECT_EQ( m_Tree.Nearest( Float2{ 20.0f, 20.0f } ), std::optional<int>( 1 ) );
    EXPECT_EQ( m_Tree.Nearest( Float2{ 70.0f, 70.0f } ), std::optional<int>( 2 ) );
}

TEST_F(QuadTreeTest, Nearest_PointInsideElement_ReturnsThatElement)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 0.0f, 0.0f, 100.0f, 100.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 60.0f, 60.0f, 2.0f, 2.0f } ).IsOk() );

    EXPECT_EQ( m_Tree.Nearest( Float2{ 61.0f, 61.0f } ).has_value(), true );
    EXPECT_EQ( m_Tree.Nearest( Float2{ 5.0f, 5.0f } ), std::optional<int>( 1 ) );
}

TEST_F(QuadTreeTest, Nearest_MatchesBruteForceDistanceOnRandomData)
{
    std::mt19937 rng( 17 );
    std::uniform_real_distribution<float> pos( 0.0f, 90.0f ), size( 0.5f, 8.0f ), big( 20.0f, 40.0f );

    std::vector<Rect> rects;
    for ( int i = 0; i < 600; ++i )
    {
        float const w = ( i % 25 == 0 ) ? big( rng ) : size( rng );
        float const h = ( i % 25 == 0 ) ? big( rng ) : size( rng );
        Rect r{ pos( rng ), pos( rng ), w, h };
        r.m_X = std::min( r.m_X, 100.0f - w );
        r.m_Y = std::min( r.m_Y, 100.0f - h );
        rects.push_back( r );
        ASSERT_TRUE( m_Tree.Insert( i, r ).IsOk() );
    }

    for ( int q = 0; q < 200; ++q )
    {
        Float2 const point{ pos( rng ) + 5.0f, pos( rng ) + 5.0f };

        float expected = std::numeric_limits<float>::infinity();
        for ( Rect const& r : rects ) expected = std::min( expected, asge::math::Distance( r, point ) );

        auto const nearest = m_Tree.Nearest( point );
        ASSERT_TRUE( nearest.has_value() );
        EXPECT_FLOAT_EQ( asge::math::Distance( rects[*nearest], point ), expected );
    }
}

// ─── Statistics ───────────────────────────────────────────────────────

TEST_F(QuadTreeTest, Stats_FreshTree_OnlyTheRoot)
{
    EXPECT_EQ( m_Tree.NodeCount(), 1u );
    EXPECT_EQ( m_Tree.MaxDepthReached(), 0 );
    EXPECT_FALSE( m_Tree.ArenaExhausted() );
    EXPECT_GT( m_Tree.ArenaUsed(), 0u );
    EXPECT_EQ( m_Tree.ArenaCapacity(), k_NodeBytes );
}

TEST_F(QuadTreeTest, Stats_SubdivisionAddsFourNodesAndOneLevel)
{
    // Spread over the four quadrants, clear of the split lines, so the first split is the only one
    for ( int i = 0; i <= QuadNode::s_MaxNofElements; ++i )
    {
        float const x = static_cast<float>( ( i % 2 ) * 50 + 5 + 8 * ( i / 4 ) );
        float const y = static_cast<float>( ( ( i / 2 ) % 2 ) * 50 + 5 );
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ x, y, 0.5f, 0.5f } ).IsOk() );
    }

    EXPECT_EQ( m_Tree.NodeCount(), 5u );
    EXPECT_EQ( m_Tree.MaxDepthReached(), 1 );
}

TEST_F(QuadTreeTest, Stats_CoincidentElements_ReachTheDepthCap)
{
    for ( int i = 0; i < 100; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 1.0f, 1.0f, 0.01f, 0.01f } ).IsOk() );
    }

    EXPECT_EQ( m_Tree.MaxDepthReached(), QuadNode::s_MaxDepth );
}

TEST_F(QuadTreeTest, Stats_ArenaUsedGrowsWithInserts)
{
    auto const before = m_Tree.ArenaUsed();
    for ( int i = 0; i < 50; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( i, Rect{ 1.0f + i, 1.0f, 0.5f, 0.5f } ).IsOk() );
    }

    EXPECT_GT( m_Tree.ArenaUsed(), before );
    EXPECT_LE( m_Tree.ArenaUsed(), m_Tree.ArenaCapacity() );
}

TEST_F(QuadTreeTest, Stats_ExhaustedArena_SetsTheFlagUntilNextReset)
{
    QuadTree<int> tree( 512, k_MaxElements );
    ASSERT_TRUE( tree.Reset( 100.0f, 100.0f ).IsOk() );

    for ( int i = 0; i < 50; ++i ) (void) tree.Insert( i, Rect{ 1.0f + i, 1.0f, 0.5f, 0.5f } );
    EXPECT_TRUE( tree.ArenaExhausted() );

    ASSERT_TRUE( tree.Reset( 100.0f, 100.0f ).IsOk() );
    EXPECT_FALSE( tree.ArenaExhausted() );
    EXPECT_EQ( tree.NodeCount(), 1u );
}

// ─── Reset validation ─────────────────────────────────────────────────

TEST_F(QuadTreeTest, Reset_InvalidSize_ReturnsInvalidArgument)
{
    float const nan = std::numeric_limits<float>::quiet_NaN();
    float const inf = std::numeric_limits<float>::infinity();

    for ( auto const& [w, h] : std::vector<std::pair<float, float>>{ { 0.0f, 10.0f }, { 10.0f, 0.0f }, { -1.0f, 10.0f },
                                                                      { 10.0f, -5.0f }, { nan, 10.0f }, { 10.0f, nan }, { inf, 10.0f } } )
    {
        auto result = m_Tree.Reset( w, h );
        ASSERT_FALSE( result.IsOk() ) << w << "x" << h;
        EXPECT_EQ( result.Code(), std::make_error_code( std::errc::invalid_argument ) );
    }
}

TEST_F(QuadTreeTest, Reset_InvalidSize_LeavesTreeUninitialised)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 1.0f, 1.0f, 1.0f, 1.0f } ).IsOk() );
    ASSERT_FALSE( m_Tree.Reset( 0.0f, 0.0f ).IsOk() );

    EXPECT_EQ( m_Tree.Size(), 0u );
    auto result = m_Tree.Insert( 2, Rect{ 1.0f, 1.0f, 1.0f, 1.0f } );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::operation_not_permitted ) );
}

TEST_F(QuadTreeTest, Reset_ArenaTooSmallForRoot_ReturnsNoBufferSpace)
{
    QuadTree<int> tree( 8, k_MaxElements );
    auto result = tree.Reset( 100.0f, 100.0f );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::no_buffer_space ) );
    EXPECT_TRUE( tree.ArenaExhausted() );
}

// ─── Reset ────────────────────────────────────────────────────────────

TEST_F(QuadTreeTest, Reset_ClearsElements)
{
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 1.0f, 1.0f, 1.0f, 1.0f } ).IsOk() );
    m_Tree.Reset( 100.0f, 100.0f );

    EXPECT_EQ( m_Tree.Size(), 0u );
    EXPECT_TRUE( Collect( Rect{ 0.0f, 0.0f, 100.0f, 100.0f } ).empty() );
}

TEST_F(QuadTreeTest, Reset_ResizesRootBounds)
{
    m_Tree.Reset( 10.0f, 10.0f );
    auto result = m_Tree.Insert( 1, Rect{ 50.0f, 50.0f, 1.0f, 1.0f } );
    ASSERT_FALSE( result.IsOk() );
    EXPECT_EQ( result.Code(), std::make_error_code( std::errc::invalid_argument ) );
}

TEST_F(QuadTreeTest, Reset_AfterHeavyUse_ReusesArenaAndCapacity)
{
    for ( int round = 0; round < 5; ++round )
    {
        m_Tree.Reset( 100.0f, 100.0f );
        for ( int i = 0; i < 500; ++i )
        {
            ASSERT_TRUE( m_Tree.Insert( i, Rect{ static_cast<float>( i % 90 ), static_cast<float>( i / 6 ), 1.0f, 1.0f } ).IsOk() );
        }
    }

    EXPECT_EQ( m_Tree.Size(), 500u );
}

}

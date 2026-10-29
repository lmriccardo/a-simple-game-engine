#include <ASGE/Core/Spatial/QuadTree.hpp>

#include <algorithm>
#include <random>
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
    for ( int i = 0; i < 9; ++i )
    {
        ASSERT_TRUE( m_Tree.Insert( 100 + i, Rect{ 40.0f, 40.0f, 20.0f, 20.0f } ).IsOk() );
    }
    ASSERT_TRUE( m_Tree.Insert( 3, Rect{ 80.0f, 80.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 2, Rect{ 10.0f, 80.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 1, Rect{ 80.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );
    ASSERT_TRUE( m_Tree.Insert( 0, Rect{ 10.0f, 10.0f, 5.0f, 5.0f } ).IsOk() );

    std::vector<int> order;
    m_Tree.Query( Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, [&order]( int v ) { order.push_back( v ); return true; } );

    ASSERT_EQ( order.size(), 13u );
    EXPECT_GE( order[0], 100 ); // the straddlers held by the root come first
    EXPECT_EQ( std::vector<int>( order.end() - 4, order.end() ), ( std::vector<int>{ 0, 1, 2, 3 } ) );
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

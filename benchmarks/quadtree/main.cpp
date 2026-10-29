#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <ASGE/Core/Spatial/QuadTree.hpp>

#include "../Bench.hpp"

namespace
{

using asge::math::Rect;
using Tree = asge::spatial::QuadTree<int>;

constexpr std::size_t k_BruteForcePairsLimit = 20000; // all-pairs brute force is O(n^2), skip above this
constexpr std::size_t k_QueryCount           = 2000;
constexpr float       k_SmallWindowSide      = 30.0f;
constexpr float       k_LargeWindowFraction  = 0.5f;  // of the world side, ~25% of the area

enum class Distribution { Uniform, Clustered, MixedSizes };

struct Options
{
    std::vector<std::size_t> m_Sizes { 1000, 5000, 20000, 100000 }; // Element counts to sweep
    int                      m_Reps  { 15 };                        // Timed repetitions per measurement
    std::string              m_CsvPath;                             // Optional CSV output file
    bool                     m_SkipBrute { false };                 // Time only the tree, no brute-force baseline or check
};

struct Checksum
{
    std::uint64_t m_Hits { 0 }; // Total number of reported elements
    std::uint64_t m_Sum  { 0 }; // Sum of their ids, to catch wrong-but-equal-count results

    bool operator==( Checksum const& ) const = default;
};

/** @brief Name of a distribution as printed in the report. */
char const* Name( Distribution inDist )
{
    switch ( inDist )
    {
        case Distribution::Uniform:    return "uniform";
        case Distribution::Clustered:  return "clustered";
        case Distribution::MixedSizes: return "mixed";
    }
    return "?";
}

/** @brief Clamps a rect origin so that origin + size <= inWorld even after float rounding. */
float FitInside( float inOrigin, float inSize, float inWorld )
{
    float origin = std::clamp( inOrigin, 0.0f, inWorld - inSize );
    while ( origin + inSize > inWorld ) origin = std::nextafter( origin, 0.0f );
    return origin;
}

/** @brief Generates inCount rects fully inside [0,inWorld]^2, deterministically for a given seed. */
std::vector<Rect> Generate( Distribution inDist, std::size_t inCount, float inWorld, std::uint32_t inSeed )
{
    std::mt19937 rng( inSeed );
    std::uniform_real_distribution<float> unit( 0.0f, 1.0f );
    std::uniform_real_distribution<float> small( 2.0f, 10.0f );
    std::uniform_real_distribution<float> large( 0.05f * inWorld, 0.10f * inWorld );

    constexpr int k_Hotspots = 8;
    std::vector<std::pair<float, float>> centers;
    for ( int i = 0; i < k_Hotspots; ++i ) centers.emplace_back( unit( rng ) * inWorld, unit( rng ) * inWorld );
    std::normal_distribution<float> spread( 0.0f, inWorld / 30.0f );

    std::vector<Rect> out;
    out.reserve( inCount );

    for ( std::size_t i = 0; i < inCount; ++i )
    {
        bool const isLarge = inDist == Distribution::MixedSizes && unit( rng ) < 0.01f;
        float const w = isLarge ? large( rng ) : small( rng );
        float const h = isLarge ? large( rng ) : small( rng );

        float x, y;
        if ( inDist == Distribution::Clustered )
        {
            auto const& c = centers[rng() % k_Hotspots];
            x = c.first + spread( rng );
            y = c.second + spread( rng );
        }
        else
        {
            x = unit( rng ) * inWorld;
            y = unit( rng ) * inWorld;
        }

        out.push_back( Rect{ FitInside( x, w, inWorld ), FitInside( y, h, inWorld ), w, h } );
    }

    return out;
}

/** @brief Random query windows of the given side, fully inside the world. */
std::vector<Rect> MakeWindows( float inSide, float inWorld, std::uint32_t inSeed )
{
    std::mt19937 rng( inSeed );
    std::uniform_real_distribution<float> unit( 0.0f, 1.0f );

    std::vector<Rect> out;
    out.reserve( k_QueryCount );
    for ( std::size_t i = 0; i < k_QueryCount; ++i )
    {
        out.push_back( Rect{ unit( rng ) * ( inWorld - inSide ), unit( rng ) * ( inWorld - inSide ), inSide, inSide } );
    }
    return out;
}

/** @brief Resets the tree and inserts every rect, returning how many inserts failed. */
std::size_t Build( Tree& ioTree, std::vector<Rect> const& inRects, float inWorld )
{
    std::size_t failures = 0;
    ioTree.Reset( inWorld, inWorld );
    for ( std::size_t i = 0; i < inRects.size(); ++i )
    {
        if ( !ioTree.Insert( static_cast<int>( i ), inRects[i] ).IsOk() ) ++failures;
    }
    return failures;
}

/** @brief Runs every window through a linear scan of all rects. */
Checksum BruteQueries( std::vector<Rect> const& inRects, std::vector<Rect> const& inWindows )
{
    Checksum sum;
    for ( Rect const& win : inWindows )
    {
        for ( std::size_t j = 0; j < inRects.size(); ++j )
        {
            if ( asge::math::AabbOverlap( inRects[j], win ) ) { ++sum.m_Hits; sum.m_Sum += j; }
        }
    }
    return sum;
}

/** @brief Runs every window through the tree. */
Checksum TreeQueries( Tree const& inTree, std::vector<Rect> const& inWindows )
{
    Checksum sum;
    for ( Rect const& win : inWindows )
    {
        inTree.Query( win, [&sum]( int id ) { ++sum.m_Hits; sum.m_Sum += static_cast<std::uint64_t>( id ); return true; } );
    }
    return sum;
}

/** @brief Counts overlapping (i, j) combinations, self-pairs included, by linear scan. */
Checksum BrutePairs( std::vector<Rect> const& inRects )
{
    Checksum sum;
    for ( std::size_t i = 0; i < inRects.size(); ++i )
    {
        for ( std::size_t j = 0; j < inRects.size(); ++j )
        {
            if ( asge::math::AabbOverlap( inRects[i], inRects[j] ) ) { ++sum.m_Hits; sum.m_Sum += j; }
        }
    }
    return sum;
}

/** @brief Same as BrutePairs but querying the tree with each rect. */
Checksum TreePairs( Tree const& inTree, std::vector<Rect> const& inRects )
{
    return TreeQueries( inTree, inRects );
}

/** @brief Counts each overlapping unordered pair once by linear scan; the sum adds both ids. */
Checksum BruteUniquePairs( std::vector<Rect> const& inRects )
{
    Checksum sum;
    for ( std::size_t i = 0; i < inRects.size(); ++i )
    {
        for ( std::size_t j = i + 1; j < inRects.size(); ++j )
        {
            if ( asge::math::AabbOverlap( inRects[i], inRects[j] ) ) { ++sum.m_Hits; sum.m_Sum += i + j; }
        }
    }
    return sum;
}

/** @brief Same checksum as BruteUniquePairs, produced by the tree's ForEachPair. */
Checksum TreeUniquePairs( Tree const& inTree )
{
    Checksum sum;
    inTree.ForEachPair( [&sum]( int a, int b ) { ++sum.m_Hits; sum.m_Sum += static_cast<std::uint64_t>( a + b ); return true; } );
    return sum;
}

struct Row
{
    std::string m_Dist;       // Distribution name
    std::size_t m_N;          // Element count
    std::string m_Scenario;   // Scenario name
    double      m_BruteMs;    // Brute force median, negative when skipped
    double      m_TreeMs;     // QuadTree median
    double      m_TreeMinMs;  // QuadTree fastest repetition
    double      m_NsPerOp;    // QuadTree median per insert/query
    bool        m_Verified;   // Tree and brute force agreed (true when brute force was skipped)
};

/** @brief Prints one row of the report table. */
void Print( Row const& inRow )
{
    char brute[24], speedup[24];
    if ( inRow.m_BruteMs >= 0.0 )
    {
        std::snprintf( brute, sizeof brute, "%10.3f", inRow.m_BruteMs );
        std::snprintf( speedup, sizeof speedup, "%7.1fx", inRow.m_BruteMs / inRow.m_TreeMs );
    }
    else
    {
        std::snprintf( brute, sizeof brute, "%10s", "-" );
        std::snprintf( speedup, sizeof speedup, "%8s", "-" );
    }

    std::printf( "%-10s %7zu  %-12s %s %10.3f %10.3f %10.1f %s  %s\n",
        inRow.m_Dist.c_str(), inRow.m_N, inRow.m_Scenario.c_str(), brute,
        inRow.m_TreeMs, inRow.m_TreeMinMs, inRow.m_NsPerOp, speedup, inRow.m_Verified ? "ok" : "MISMATCH" );
}

/** @brief Parses --sizes a,b,c  --reps N  --csv file  --no-brute. Returns false on a bad argument. */
bool ParseArgs( int inArgc, char** inArgv, Options& outOptions )
{
    for ( int i = 1; i < inArgc; ++i )
    {
        std::string const arg = inArgv[i];
        bool const hasValue = i + 1 < inArgc;

        if ( arg == "--reps" && hasValue ) outOptions.m_Reps = std::atoi( inArgv[++i] );
        else if ( arg == "--csv" && hasValue ) outOptions.m_CsvPath = inArgv[++i];
        else if ( arg == "--no-brute" ) outOptions.m_SkipBrute = true;
        else if ( arg == "--sizes" && hasValue )
        {
            outOptions.m_Sizes.clear();
            std::string list = inArgv[++i];
            std::size_t pos = 0;
            while ( pos < list.size() )
            {
                std::size_t const comma = list.find( ',', pos );
                outOptions.m_Sizes.push_back( std::stoull( list.substr( pos, comma - pos ) ) );
                if ( comma == std::string::npos ) break;
                pos = comma + 1;
            }
        }
        else
        {
            std::fprintf( stderr, "usage: quadtree_bench [--sizes 1000,5000] [--reps 15] [--csv out.csv] [--no-brute]\n" );
            return false;
        }
    }
    return outOptions.m_Reps > 0 && !outOptions.m_Sizes.empty();
}

/** @brief Runs all scenarios for one distribution and element count, appending to outRows. */
void RunCase( Distribution inDist, std::size_t inN, Options const& inOptions, std::vector<Row>& outRows )
{
    float const world = std::sqrt( static_cast<float>( inN ) ) * 10.0f; // keeps density constant across N
    auto const rects = Generate( inDist, inN, world, 42 );
    auto const smallWindows = MakeWindows( k_SmallWindowSide, world, 7 );
    auto const largeWindows = MakeWindows( world * k_LargeWindowFraction, world, 8 );

    Tree tree( inN * 512 + ( 1u << 20 ), inN );
    std::string const dist = Name( inDist );
    int const warmup = 2;
    int const bruteReps = std::max( 3, inOptions.m_Reps / 5 ); // brute force is slow and low-variance

    auto const add = [&]( std::string inScenario, double inBruteNs, bench::Stats inTree, double inOps, bool inOk )
    {
        outRows.push_back( Row{ dist, inN, std::move( inScenario ), inBruteNs < 0 ? -1.0 : inBruteNs / 1e6,
            inTree.m_MedianNs / 1e6, inTree.m_MinNs / 1e6, inTree.m_MedianNs / inOps, inOk } );
        Print( outRows.back() );
    };

    std::size_t failures = 0;
    auto const build = bench::Measure( warmup, inOptions.m_Reps, [&] { failures += Build( tree, rects, world ); } );
    add( "build", -1.0, build, static_cast<double>( inN ), failures == 0 );
    std::printf( "# arena %.1f of %.1f MiB used, %zu nodes, depth %u%s\n",
        static_cast<double>( tree.ArenaUsed() ) / ( 1 << 20 ), static_cast<double>( tree.ArenaCapacity() ) / ( 1 << 20 ),
        tree.NodeCount(), static_cast<unsigned>( tree.MaxDepthReached() ), tree.ArenaExhausted() ? " (EXHAUSTED)" : "" );

    auto const queryScenario = [&]( char const* inName, std::vector<Rect> const& inWindows )
    {
        Checksum brute, fromTree;
        if ( inOptions.m_SkipBrute )
        {
            auto const only = bench::Measure( warmup, inOptions.m_Reps, [&] { fromTree = TreeQueries( tree, inWindows ); } );
            add( inName, -1.0, only, static_cast<double>( inWindows.size() ), true );
            return;
        }

        auto const bruteStats = bench::Measure( 1, bruteReps, [&] { brute = BruteQueries( rects, inWindows ); } );
        auto const treeStats  = bench::Measure( warmup, inOptions.m_Reps, [&] { fromTree = TreeQueries( tree, inWindows ); } );
        add( inName, bruteStats.m_MedianNs, treeStats, static_cast<double>( inWindows.size() ), brute == fromTree );
    };

    queryScenario( "query-small", smallWindows );
    queryScenario( "query-large", largeWindows );

    Checksum pairsTree, pairsBrute;
    auto const pairsTreeStats = bench::Measure( warmup, inOptions.m_Reps, [&] { pairsTree = TreePairs( tree, rects ); } );
    Checksum uniqueTree, uniqueBrute;
    auto const uniqueStats = bench::Measure( warmup, inOptions.m_Reps, [&] { uniqueTree = TreeUniquePairs( tree ); } );
    if ( inN <= k_BruteForcePairsLimit && !inOptions.m_SkipBrute )
    {
        auto const uniqueBruteStats = bench::Measure( 1, bruteReps, [&] { uniqueBrute = BruteUniquePairs( rects ); } );
        add( "pairs", uniqueBruteStats.m_MedianNs, uniqueStats, static_cast<double>( inN ), uniqueBrute == uniqueTree );
    }
    else
    {
        // Every ordered pair appears twice in all-pairs, plus one self hit per rect
        Checksum expected;
        expected.m_Hits = ( pairsTree.m_Hits - inN ) / 2;
        expected.m_Sum  = pairsTree.m_Sum - static_cast<std::uint64_t>( inN ) * ( inN - 1 ) / 2;
        add( "pairs", -1.0, uniqueStats, static_cast<double>( inN ), expected == uniqueTree );
    }

    if ( inN <= k_BruteForcePairsLimit && !inOptions.m_SkipBrute )
    {
        auto const pairsBruteStats = bench::Measure( 1, bruteReps, [&] { pairsBrute = BrutePairs( rects ); } );
        add( "all-pairs", pairsBruteStats.m_MedianNs, pairsTreeStats, static_cast<double>( inN ), pairsBrute == pairsTree );
    }
    else
    {
        add( "all-pairs", -1.0, pairsTreeStats, static_cast<double>( inN ), true );
    }
}

/** @brief Writes all rows to a CSV file for before/after comparisons. */
void WriteCsv( std::string const& inPath, std::vector<Row> const& inRows )
{
    std::FILE* file = std::fopen( inPath.c_str(), "w" );
    if ( !file )
    {
        std::fprintf( stderr, "cannot open %s for writing\n", inPath.c_str() );
        return;
    }

    std::fprintf( file, "distribution,n,scenario,brute_ms,tree_ms,tree_min_ms,tree_ns_per_op,verified\n" );
    for ( Row const& r : inRows )
    {
        std::fprintf( file, "%s,%zu,%s,%.4f,%.4f,%.4f,%.2f,%d\n", r.m_Dist.c_str(), r.m_N, r.m_Scenario.c_str(),
            r.m_BruteMs, r.m_TreeMs, r.m_TreeMinMs, r.m_NsPerOp, r.m_Verified ? 1 : 0 );
    }
    std::fclose( file );
}

}

int main( int argc, char** argv )
{
    Options options;
    if ( !ParseArgs( argc, argv, options ) ) return 2;

#ifndef NDEBUG
    std::fprintf( stderr, "WARNING: unoptimized build, timings are not meaningful. Use the *-bench preset.\n" );
#endif

    std::printf( "%-10s %7s  %-12s %10s %10s %10s %10s %8s  %s\n",
        "dist", "N", "scenario", "brute ms", "tree ms", "tree min", "ns/op", "speedup", "check" );

    std::vector<Row> rows;
    for ( Distribution dist : { Distribution::Uniform, Distribution::Clustered, Distribution::MixedSizes } )
    {
        for ( std::size_t n : options.m_Sizes ) RunCase( dist, n, options, rows );
    }

    if ( !options.m_CsvPath.empty() ) WriteCsv( options.m_CsvPath, rows );

    bool const allOk = std::all_of( rows.begin(), rows.end(), []( Row const& r ) { return r.m_Verified; } );
    return allOk ? 0 : 1;
}

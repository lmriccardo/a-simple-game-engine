#include "CatmullRomSpline.hpp"

float asge::math::BuildArcLengthTable(
    Float2 const &inP0, Float2 const &inP1, Float2 const &inP2,
    Float2 const &inP3, std::size_t inResolution, std::vector<ArcLengthSample> &outTable) noexcept
{
    outTable.clear();
    outTable.emplace_back(0.0f, 0.0f);

    Float2 prevPoint = Hermite( inP0, inP1, inP2, inP3, 0.0f );
    float accumulated = 0.0f;

    for ( std::size_t s = 1; s <= inResolution; ++s )
    {
        float t = static_cast<float>(s) / static_cast<float>(inResolution);
        Float2 currPoint = Hermite( inP0, inP1, inP2, inP3, t );
        accumulated += Length( ( currPoint - prevPoint ) );
        outTable.emplace_back( t, accumulated );
        prevPoint = currPoint;
    }

    return accumulated;
}

asge::math::Float2 asge::math::Hermite(
    Float2 const &inP0, Float2 const &inP1, Float2 const &inP2, 
    Float2 const &inP3, float inT) noexcept
{
    const float t2 = inT * inT;
    const float t3 = t2  * inT;

    const Float2 m1 = ( inP2 - inP0 ) * 0.5f;
    const Float2 m2 = ( inP3 - inP1 ) * 0.5f;

    const float h00 =  2*t3 - 3*t2 + 1;
    const float h10 =    t3 - 2*t2 + inT;
    const float h01 = -2*t3 + 3*t2;
    const float h11 =    t3 -   t2;

    return inP1*h00 + m1*h10 + inP2*h01 + m2*h11;
}

asge::math::Float2 asge::math::HermiteDerivative(
    Float2 const &inP0, Float2 const &inP1, Float2 const &inP2, 
    Float2 const &inP3, float inT) noexcept
{
    // 2. tangent vectors at the segment's endpoints
    Float2 m1 = ( inP2 - inP0 ) * 0.5f;
    Float2 m2 = ( inP3 - inP1 ) * 0.5f;

    // 3. derivative of the Hermite basis, evaluated at localT
    float t2   =  inT  * inT;
    float dh00 =  6*t2 - 6*inT;
    float dh10 =  3*t2 - 4*inT + 1;
    float dh01 = -6*t2 + 6*inT;
    float dh11 =  3*t2 - 2*inT;

    Float2 tangent = inP1*dh00 + m1*dh10 + inP2*dh01 + m2*dh11;
    return tangent;
}

std::pair<std::size_t, float> asge::math::CatmullRomSpline::LocateSegment(float inTime) const
{
    inTime = std::clamp(inTime, 0.0f, 1.0f);

    // 1. map global t -> (segment index, local t within that segment)
    const std::size_t n = m_Segments.size();
    float scaled = inTime * static_cast<float>(n);
    std::size_t segIdx = std::min( static_cast<std::size_t>(scaled), n - 1 );
    float localT = scaled - static_cast<float>(segIdx);

    return {segIdx, localT};
}

std::pair<std::size_t, float> asge::math::CatmullRomSpline::LocateByDistance(float inDistance) const noexcept
{
    inDistance = std::clamp( inDistance, 0.0f, m_TotalLength );

    // 1. Find the segment containing this distance
    auto segIt = std::upper_bound(
        m_Segments.begin(), m_Segments.end(), inDistance,
        []( float d, Segment const& seg ){ return d < seg.m_StartDistance + seg.m_Length; }
    );

    if (segIt == m_Segments.end()) segIt = std::prev(m_Segments.end());

    // 2. Find t within this segment's local arc-length table that matches localDist
    Segment const& seg = *segIt;
    const std::size_t segIdx = static_cast<std::size_t>(std::distance(m_Segments.begin(), segIt));

    float localDist = inDistance - seg.m_StartDistance;
    auto sampleIt = std::upper_bound(
        seg.m_Table.begin(), seg.m_Table.end(), localDist,
        []( float d, ArcLengthSample const& sample ){ return d < sample.m_Distance; }
    );

    float localT;
    if      ( sampleIt == seg.m_Table.begin() ) localT = 0.0f;
    else if ( sampleIt == seg.m_Table.end() )   localT = 1.0f;
    else {
        ArcLengthSample const& prev = *std::prev( sampleIt );
        ArcLengthSample const& curr = *sampleIt;
        float segFrac = ( localDist - prev.m_Distance ) / ( curr.m_Distance - prev.m_Distance );
        localT = prev.m_T + segFrac * ( curr.m_T - prev.m_T );
    }

    return { segIdx, localT };
}

asge::math::CatmullRomSpline::CatmullRomSpline(
    std::vector<Float2> inWp, std::size_t inResolution)
    : m_Waypoints(std::move(inWp))
{
    const int n = static_cast<int>(m_Waypoints.size());
    if ( n < 2 ) return;

    // Construct all the segments in the ctor rather than lazy evaluation
    m_Segments.reserve( n - 1 );
    float accumulated { 0.0f };

    for ( int ii = 0; ii < n - 1; ++ii )
    {
        Segment seg;
        seg.m_P0 = m_Waypoints[std::max(ii - 1, 0)];
        seg.m_P1 = m_Waypoints[ii];
        seg.m_P2 = m_Waypoints[ii + 1];
        seg.m_P3 = m_Waypoints[std::min(ii + 2, n - 1)];

        // Build this segment's local arc-length table
        seg.m_Table.reserve( inResolution + 1 );
        seg.m_Length = BuildArcLengthTable(
            seg.m_P0, seg.m_P1, seg.m_P2, seg.m_P3, inResolution, seg.m_Table
        );

        seg.m_StartDistance = accumulated;
        accumulated += seg.m_Length;
        m_Segments.push_back( std::move(seg) );
    }

    m_TotalLength = accumulated;
}

std::vector<asge::math::CatmullRomSpline::Segment> const &
asge::math::CatmullRomSpline::Segments() const noexcept
{
    return m_Segments;
}

std::vector<asge::math::Float2> const &
asge::math::CatmullRomSpline::Waypoints() const noexcept
{
    return m_Waypoints;
}

bool asge::math::CatmullRomSpline::HasSegments() const noexcept
{
    return !m_Segments.empty();
}

float asge::math::CatmullRomSpline::Length() const noexcept
{
    return m_TotalLength;
}

asge::math::Float2 asge::math::CatmullRomSpline::PointAt(float inTime) const noexcept
{
    if ( m_Segments.empty() ) return Float2{};
    auto [ segIdx, localT ] = LocateSegment( inTime );
    Segment const& seg = m_Segments[ segIdx ];
    return Hermite( seg.m_P0, seg.m_P1, seg.m_P2, seg.m_P3, localT );
}

asge::math::Float2 asge::math::CatmullRomSpline::PointAtDistance(float inDistance) const noexcept
{
    if ( m_Segments.empty() ) return Float2{};
    auto [ segIdx, localT ] = LocateByDistance( inDistance );
    auto const& seg = m_Segments[segIdx];
    return Hermite( seg.m_P0, seg.m_P1, seg.m_P2, seg.m_P3, localT );
}

asge::math::Float2 asge::math::CatmullRomSpline::TangentAt(float inTime) const noexcept
{
    if ( m_Segments.empty() ) return Float2{};
    auto [ segIdx, localT ] = LocateSegment( inTime );
    Segment const& seg = m_Segments[ segIdx ];
    return NormalizeVec( HermiteDerivative( 
        seg.m_P0, seg.m_P1, seg.m_P2, seg.m_P3, localT ) );
}

float asge::math::CatmullRomSpline::TimeAtDistance(float inDistance) const noexcept
{
    if (m_Segments.empty()) return 0.0f;
    auto [segIdx, localT] = LocateByDistance(inDistance);
    return (static_cast<float>(segIdx) + localT) / static_cast<float>(m_Segments.size());
}

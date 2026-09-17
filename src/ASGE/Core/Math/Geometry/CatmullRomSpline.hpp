#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

/** @brief One entry of a segment's arc-length lookup table, mapping a Hermite
 *  parameter `m_T` to the cumulative distance travelled along the curve
 *  from `t=0` up to that point. */
struct ArcLengthSample
{
    float m_T{0.0f};        // Hermite parameter in [0, 1] this sample was taken at
    float m_Distance{0.0f}; // Cumulative arc length from t=0 up to m_T
};

/** @brief Samples a Hermite segment at `inResolution` evenly spaced steps and
 *  appends the resulting (t, cumulative distance) pairs to `outTable`.
 *  Returns the segment's total arc length. */
float BuildArcLengthTable(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2,
    Float2 const& inP3, std::size_t inResolution, std::vector<ArcLengthSample>& outTable
) noexcept;

/** @brief Evaluates a Catmull-Rom segment, expressed via its Hermite basis,
 *  at parameter `inT` in [0, 1], where `inP1`/`inP2` are the segment's
 *  endpoints and `inP0`/`inP3` are the neighbouring waypoints used to
 *  derive the endpoint tangents. */
Float2 Hermite(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2,
    Float2 const& inP3, float inT
) noexcept;

/** @brief Derivative of Hermite() with respect to `inT`, i.e. the (non
 *  normalized) tangent vector of the segment at parameter `inT`. */
Float2 HermiteDerivative(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2,
    Float2 const& inP3, float inT
) noexcept;

/** @brief A piecewise Catmull-Rom spline through a list of waypoints, with
 *  each segment pre-sampled into an arc-length table so points and tangents
 *  can be looked up either by a normalized `[0, 1]` curve parameter or by
 *  actual distance travelled along the curve. */
class CatmullRomSpline
{
    /** @brief One Hermite segment between waypoints `m_P1` and `m_P2`, plus
     *  its precomputed arc-length table and where it starts along the
     *  overall spline. */
    struct Segment
    {
        Float2 m_P0, m_P1, m_P2, m_P3;        // Segment endpoints (P1, P2) and neighbours (P0, P3) used for tangents
        std::vector<ArcLengthSample> m_Table; // Arc-length lookup table local to this segment
        float m_StartDistance;                // Cumulative spline distance where this segment begins
        float m_Length;                       // This segment's own arc length
    };

    std::vector<Float2>  m_Waypoints{};
    std::vector<Segment> m_Segments{};
    float                m_TotalLength{0.0f};

    /** @brief Maps a normalized spline parameter `inTime` in [0, 1] to the
     *  segment it falls in and the local Hermite parameter within it. */
    std::pair<std::size_t, float> LocateSegment( float inTime ) const;

    /** @brief Maps `inDistance` (clamped to [0, Length()]) to the segment it
     *  falls in and the local Hermite parameter within it, via that
     *  segment's arc-length table — the distance-based counterpart to
     *  LocateSegment(). Shared by PointAtDistance() and TimeAtDistance(). */
    std::pair<std::size_t, float> LocateByDistance(float inDistance) const noexcept;
public:
    CatmullRomSpline() = default;

    /** @brief Builds the spline through `inWp`, precomputing each segment's
     *  arc-length table with `inResolution` samples. Splines with fewer
     *  than two waypoints have no segments and evaluate to a zero point. */
    explicit CatmullRomSpline(
        std::vector<Float2> inWp, std::size_t inResolution=32 );

    CatmullRomSpline( CatmullRomSpline const& ) = default;
    CatmullRomSpline( CatmullRomSpline && ) = default;
    CatmullRomSpline& operator=( CatmullRomSpline const& ) = default;
    CatmullRomSpline& operator=( CatmullRomSpline && ) = default;

    ~CatmullRomSpline() = default;

    /** @brief Returns the spline's precomputed segments, in order. */
    std::vector<Segment> const& Segments () const noexcept;

    /** @brief Returns the waypoints the spline was constructed from. */
    std::vector<Float2>  const& Waypoints() const noexcept;

    /** @brief Check if the spline has segments in it. */
    bool HasSegments() const noexcept;

    /** @brief Returns the total arc length of the spline. */
    float Length() const noexcept;

    /** @brief Returns the point at normalized parameter `inTime`, clamped
     *  to [0, 1] and distributed evenly across segments (not arc-length
     *  corrected). */
    Float2 PointAt( float inTime ) const noexcept;

    /** @brief Returns the point at `inDistance` travelled along the curve,
     *  clamped to [0, Length()], using each segment's arc-length table. */
    Float2 PointAtDistance( float inDistance ) const noexcept;

    /** @brief Returns the normalized tangent direction at normalized
     *  parameter `inTime`. */
    Float2 TangentAt( float inTime ) const noexcept;

    /** @brief Returns the normalized `[0, 1]` parameter TangentAt()/PointAt()
     *  expect for the point `inDistance` (clamped to [0, Length()]) along
     *  the curve — the inverse of the distribution PointAt() walks, so
     *  `PointAt(TimeAtDistance(d))` matches `PointAtDistance(d)`. Returns 0
     *  for a spline with no segments. */
    float TimeAtDistance( float inDistance ) const noexcept;
};

}
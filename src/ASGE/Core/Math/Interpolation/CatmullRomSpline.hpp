#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

/**
 * @brief One entry of a segment's arc-length lookup table.
 *
 * Maps a Hermite parameter `m_T` to the cumulative distance travelled along
 * the curve from `t=0` up to that point.
 */
struct ArcLengthSample
{
    float m_T{0.0f};        // Hermite parameter in [0, 1] this sample was taken at
    float m_Distance{0.0f}; // Cumulative arc length from t=0 up to m_T
};

/**
 * @brief Samples a Hermite segment into an arc-length table.
 *
 * Clears outTable, then fills it with inResolution + 1 evenly spaced (t,
 * cumulative distance) pairs starting at (0, 0). Lengths are summed over
 * straight chords, so a higher resolution is more accurate. Returns the
 * segment's total arc length.
 */
float BuildArcLengthTable(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2,
    Float2 const& inP3, std::size_t inResolution, std::vector<ArcLengthSample>& outTable
) noexcept;

/**
 * @brief Evaluates a Catmull-Rom segment, expressed via its Hermite basis.
 *
 * inT is in [0, 1] (not clamped), where `inP1`/`inP2` are the segment's
 * endpoints and `inP0`/`inP3` are the neighbouring waypoints used to derive
 * the endpoint tangents.
 */
Float2 Hermite(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2,
    Float2 const& inP3, float inT
) noexcept;

/**
 * @brief Derivative of Hermite() with respect to `inT`.
 *
 * It is the (non normalized) tangent vector of the segment at parameter
 * `inT`, taking the same four points as Hermite().
 */
Float2 HermiteDerivative(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2,
    Float2 const& inP3, float inT
) noexcept;

/**
 * @brief A piecewise Catmull-Rom spline through a list of waypoints.
 *
 * Each segment is pre-sampled into an arc-length table so points and tangents
 * can be looked up either by a normalized `[0, 1]` curve parameter or by
 * actual distance travelled along the curve. The curve passes through every
 * waypoint; the first and last waypoints are reused as their own neighbours.
 */
class CatmullRomSpline
{
    // One Hermite segment between waypoints `m_P1` and `m_P2`, plus its
    // precomputed arc-length table and where it starts along the overall spline.
    struct Segment
    {
        Float2 m_P0, m_P1, m_P2, m_P3;        // Segment endpoints (P1, P2) and neighbours (P0, P3) used for tangents
        std::vector<ArcLengthSample> m_Table; // Arc-length lookup table local to this segment
        float m_StartDistance;                // Cumulative spline distance where this segment begins
        float m_Length;                       // This segment's own arc length
    };

    std::vector<Float2>  m_Waypoints{};       // The points the curve passes through
    std::vector<Segment> m_Segments{};        // One segment per pair of consecutive waypoints
    float                m_TotalLength{0.0f}; // Arc length of the whole spline

    /**
     * @brief Maps a normalized spline parameter to its segment and local parameter.
     *
     * `inTime` is clamped to [0, 1] and spread evenly over the segments, then
     * split into the segment index and the Hermite parameter within it. Only
     * call it when there is at least one segment.
     */
    std::pair<std::size_t, float> LocateSegment( float inTime ) const;

    /**
     * @brief Maps a travelled distance to its segment and local parameter.
     *
     * `inDistance` is clamped to [0, Length()], then resolved through that
     * segment's arc-length table with linear interpolation -- the
     * distance-based counterpart to LocateSegment(). Shared by
     * PointAtDistance() and TimeAtDistance(); only call it when there is at
     * least one segment.
     */
    std::pair<std::size_t, float> LocateByDistance(float inDistance) const noexcept;
public:
    /**
     * @brief Creates an empty spline with no segments.
     */
    CatmullRomSpline() = default;

    /**
     * @brief Builds the spline through `inWp`.
     *
     * Precomputes each segment's arc-length table with `inResolution`
     * samples. Splines with fewer than two waypoints have no segments and
     * evaluate to a zero point.
     */
    explicit CatmullRomSpline(
        std::vector<Float2> inWp, std::size_t inResolution=32 );

    CatmullRomSpline( CatmullRomSpline const& ) = default;
    CatmullRomSpline( CatmullRomSpline && ) = default;
    CatmullRomSpline& operator=( CatmullRomSpline const& ) = default;
    CatmullRomSpline& operator=( CatmullRomSpline && ) = default;

    ~CatmullRomSpline() = default;

    /**
     * @brief Returns the spline's precomputed segments, in order.
     *
     * One fewer than the number of waypoints, or none for fewer than two.
     */
    std::vector<Segment> const& Segments () const noexcept;

    /**
     * @brief Returns the waypoints the spline was constructed from.
     */
    std::vector<Float2>  const& Waypoints() const noexcept;

    /**
     * @brief Check if the spline has segments in it.
     *
     * False for fewer than two waypoints, in which case every lookup returns
     * a zero point.
     */
    bool HasSegments() const noexcept;

    /**
     * @brief Returns the total arc length of the spline.
     *
     * Approximated from the arc-length tables, so it depends on the
     * resolution given at construction. 0 for an empty spline.
     */
    float Length() const noexcept;

    /**
     * @brief Returns the point at normalized parameter `inTime`.
     *
     * `inTime` is clamped to [0, 1] and distributed evenly across segments
     * (not arc-length corrected), so a long segment is traversed faster than a
     * short one. Use PointAtDistance() for constant speed.
     */
    Float2 PointAt( float inTime ) const noexcept;

    /**
     * @brief Returns the point at `inDistance` travelled along the curve.
     *
     * `inDistance` is clamped to [0, Length()] and resolved with each
     * segment's arc-length table, giving constant speed along the curve.
     */
    Float2 PointAtDistance( float inDistance ) const noexcept;

    /**
     * @brief Returns the normalized tangent direction at normalized parameter `inTime`.
     *
     * Uses the same parameter mapping as PointAt(). Zero for an empty spline;
     * where the curve has a zero-length tangent (repeated waypoints) the
     * result is NaN.
     */
    Float2 TangentAt( float inTime ) const noexcept;

    /**
     * @brief Converts a travelled distance into the parameter PointAt() expects.
     *
     * Returns the normalized `[0, 1]` parameter for the point `inDistance`
     * (clamped to [0, Length()]) along the curve -- the inverse of the
     * distribution PointAt() walks, so `PointAt(TimeAtDistance(d))` matches
     * `PointAtDistance(d)`. Returns 0 for a spline with no segments.
     */
    float TimeAtDistance( float inDistance ) const noexcept;
};

}
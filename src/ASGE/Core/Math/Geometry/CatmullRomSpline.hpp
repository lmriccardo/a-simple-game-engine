#pragma once

#include <vector>
#include <cstdint>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace asge::math
{

struct ArcLengthSample 
{ 
    float m_T{0.0f}; 
    float m_Distance{0.0f}; 
};

float BuildArcLengthTable(
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2, 
    Float2 const& inP3, std::vector<ArcLengthSample>& outTable
) noexcept;

Float2 Hermite( 
    Float2 const& inP0, Float2 const& inP1, Float2 const& inP2, 
    Float2 const& inP3, float inT 
) noexcept;

class CatmullRomSpline
{
    struct Segment
    {
        Float2 m_P0, m_P1, m_P2, m_P3;
        std::vector<ArcLengthSample> m_Table;
        float m_StartDistance;
        float m_Length;
    };

    std::vector<Float2>  m_Waypoints;
    std::vector<Segment> m_Segments;
    float                m_TotalLength{0.0f};
public:
    explicit CatmullRomSpline( std::vector<Float2> inWp, std::size_t inResolution=32 );

};

}
#include "CatmullRomSpline.hpp"

float asge::math::BuildArcLengthTable(
    Float2 const &inP0, Float2 const &inP1, Float2 const &inP2, 
    Float2 const &inP3, std::vector<ArcLengthSample> &outTable) noexcept
{
    auto it = outTable.begin();
    outTable.emplace(it, 0.0f, 0.0f);
    
    Float2 prevPoint = Hermite( inP0, inP1, inP2, inP3, 0.0f );
    float accumulated = 0.0f;
    std::size_t samples = outTable.size() - 1;

    for ( std::size_t s = 0; s <= samples; ++s )
    {
        it = std::next( it );
        float t = static_cast<float>(s) / samples;
        Float2 currPoint = Hermite( inP0, inP1, inP2, inP3, t );
        accumulated += Length( ( currPoint - prevPoint ) );
        outTable.emplace( it, t, accumulated );
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

asge::math::CatmullRomSpline::CatmullRomSpline(
    std::vector<Float2> inWp, std::size_t inResolution)
{
    
}

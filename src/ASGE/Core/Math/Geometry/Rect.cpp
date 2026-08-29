#include "Rect.hpp"

bool asge::math::AabbOverlap(Rect const &inA, Rect const &inB) noexcept
{
    return inA.x < inB.x + inB.w && inA.x + inA.w > inB.x
        && inA.y < inB.y + inB.h && inA.y + inA.h > inB.y;
}

std::optional<asge::math::Float2> asge::math::PenetrationVector(Rect const &inA, Rect const &inB) noexcept
{
    if ( !AabbOverlap( inA, inB ) ) return std::nullopt;

    float const overlapX = std::min( inA.x + inA.w, inB.x + inB.w ) - std::max( inA.x, inB.x );
    float const overlapY = std::min( inA.y + inA.h, inB.y + inB.h ) - std::max( inA.y, inB.y );

    float const aCenterX = inA.x + inA.w * 0.5f;
    float const bCenterX = inB.x + inB.w * 0.5f;
    float const aCenterY = inA.y + inA.h * 0.5f;
    float const bCenterY = inB.y + inB.h * 0.5f;

    if ( overlapX < overlapY )
    {
        float const sign = aCenterX < bCenterX ? -1.0f : 1.0f;
        return Float2{ overlapX * sign, 0.0f };
    }

    float const sign = aCenterY < bCenterY ? -1.0f : 1.0f;
    return Float2{ 0.0f, overlapY * sign };
}

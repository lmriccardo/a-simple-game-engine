#include "Collision.hpp"

#include <algorithm>
#include <cmath>

bool asge::math::AabbOverlap(Rect const &inA, Rect const &inB) noexcept
{
    return inA.x < inB.x + inB.w && inA.x + inA.w > inB.x
        && inA.y < inB.y + inB.h && inA.y + inA.h > inB.y;
}

bool asge::math::CircleOverlap(Circle const &inA, Circle const &inB) noexcept
{
    float const dx = inB.m_Center.x() - inA.m_Center.x();
    float const dy = inB.m_Center.y() - inA.m_Center.y();
    float const rSum = inA.m_Radius + inB.m_Radius;
    return ( dx * dx + dy * dy ) < ( rSum * rSum );
}

std::optional<asge::math::Float2> asge::math::PenetrationVector(Circle const &inA, Circle const &inB) noexcept
{
    if ( !CircleOverlap( inA, inB ) ) return std::nullopt;

    float const dx = inB.m_Center.x() - inA.m_Center.x();
    float const dy = inB.m_Center.y() - inA.m_Center.y();
    float const rSum = inA.m_Radius + inB.m_Radius;
    float const dSq = dx * dx + dy * dy;

    float const distance = std::sqrt( dSq );
    if ( distance == 0.0f ) return Float2{ rSum, 0.0f };

    float const depth = rSum - distance;
    return Float2{ ( dx / distance ) * depth, ( dy / distance ) * depth };
}

std::optional<asge::math::Float2> asge::math::PenetrationVector(Rect const &inRect, Circle const &inCircle) noexcept
{
    float const closestX = std::clamp( inCircle.m_Center.x(), inRect.x, inRect.x + inRect.w );
    float const closestY = std::clamp( inCircle.m_Center.y(), inRect.y, inRect.y + inRect.h );

    float const dx = inCircle.m_Center.x() - closestX;
    float const dy = inCircle.m_Center.y() - closestY;
    float const distanceSq = dx * dx + dy * dy;

    if ( distanceSq >= inCircle.m_Radius * inCircle.m_Radius ) return std::nullopt;

    if ( distanceSq > 0.0f )
    {
        // Center is outside the rect (but within the radius of its nearest
        // edge/corner) -- push the rect away along the center-to-closest-
        // point direction.
        float const distance = std::sqrt( distanceSq );
        float const depth = inCircle.m_Radius - distance;
        return Float2{ -( dx / distance ) * depth, -( dy / distance ) * depth };
    }

    // Center is inside the rect -- "closest point on the rect" degenerates
    // to the center itself, so the direction above is undefined. Push the
    // rect away along whichever of its four edges is nearest to the
    // center, rather than assuming "up".
    float const leftPen   = ( inCircle.m_Center.x() - inRect.x ) + inCircle.m_Radius;
    float const rightPen  = ( inRect.x + inRect.w - inCircle.m_Center.x() ) + inCircle.m_Radius;
    float const topPen    = ( inCircle.m_Center.y() - inRect.y ) + inCircle.m_Radius;
    float const bottomPen = ( inRect.y + inRect.h - inCircle.m_Center.y() ) + inCircle.m_Radius;

    float const minPen = std::min( { leftPen, rightPen, topPen, bottomPen } );

    if ( minPen == leftPen )   return Float2{  leftPen,  0.0f }; // push rect right, away from the left edge
    if ( minPen == rightPen )  return Float2{ -rightPen, 0.0f }; // push rect left, away from the right edge
    if ( minPen == topPen )    return Float2{ 0.0f,  topPen };   // push rect down, away from the top edge
    return Float2{ 0.0f, -bottomPen };                           // push rect up, away from the bottom edge
}

std::optional<asge::math::Float2> asge::math::PenetrationVector(Circle const &inCircle, Rect const &inRect) noexcept
{
    auto opposite = PenetrationVector( inRect, inCircle );
    if ( !opposite ) return std::nullopt;
    return Float2{ -opposite->x(), -opposite->y() }; // flip: now pushes Circle away from Rect
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

#include "Rect.hpp"

#include <algorithm>
#include <cmath>

bool asge::math::Contains(Rect const &inRect, Float2 const &inP) noexcept
{
    return ( inP.x() > inRect.m_X && inP.x() < inRect.m_X + inRect.m_Width  )
        && ( inP.y() > inRect.m_Y && inP.y() < inRect.m_Y + inRect.m_Height );
}

bool asge::math::Contains(Rect const &inRect1, Rect const &inRect2) noexcept
{
    return inRect2.m_X >= inRect1.m_X && inRect2.m_X + inRect2.m_Width  <= inRect1.m_X + inRect1.m_Width
        && inRect2.m_Y >= inRect1.m_Y && inRect2.m_Y + inRect2.m_Height <= inRect1.m_Y + inRect1.m_Height;
}

float asge::math::Distance(Rect const &inRect, Float2 const &inP) noexcept
{
    float const closestX = std::clamp( inP.x(), inRect.m_X, inRect.m_X + inRect.m_Width );
    float const closestY = std::clamp( inP.y(), inRect.m_Y, inRect.m_Y + inRect.m_Height );
    return Distance( Float2{ closestX, closestY }, inP );
}

asge::math::Rect asge::math::Merge(Rect const &inR1, Rect const &inR2) noexcept
{
    float const startX = std::min( inR1.m_X, inR2.m_X );
    float const startY = std::min( inR1.m_Y, inR2.m_Y );
    float const stopX  = std::max( inR1.m_X + inR1.m_Width, inR2.m_X + inR2.m_Width ); 
    float const stopY  = std::max( inR1.m_Y + inR1.m_Height, inR2.m_Y + inR2.m_Height );

    return Rect{ startX, startY, stopX - startX, stopY - startY };
}

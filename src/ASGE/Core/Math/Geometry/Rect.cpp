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

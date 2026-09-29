#include "Rect.hpp"

bool asge::math::Contains(Rect const &inRect, Float2 const &inP) noexcept
{
    return ( inP.x() > inRect.m_X && inP.x() < inRect.m_X + inRect.m_Width  )
        && ( inP.y() > inRect.m_Y && inP.y() < inRect.m_Y + inRect.m_Height );
}
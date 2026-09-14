#include "Camera.hpp"

asge::math::Float2 asge::video::WorldToScreen(
    Camera const &inCamera, math::Float2 inWorldPos) noexcept
{
    return math::Float2{
        (inWorldPos.x() - inCamera.m_X) * inCamera.m_Zoom,
        (inWorldPos.y() - inCamera.m_Y) * inCamera.m_Zoom
    };
}

asge::math::Float2 asge::video::WorldToScreen(
    Camera const &inCamera, Viewport const &inViewport, math::Float2 inWorldPos) noexcept
{
    math::Float2 const local = WorldToScreen(inCamera, inWorldPos);
    return math::Float2{ local.x() + inViewport.m_X, local.y() + inViewport.m_Y };
}

asge::math::Float2 asge::video::ScreenToWorld(
    Camera const &inCamera, Viewport const &inViewport, math::Float2 inScreenPos) noexcept
{
    math::Float2 const local{ 
        inScreenPos.x() - inViewport.m_X, inScreenPos.y() - inViewport.m_Y
    };

    return math::Float2{
        local.x() / inCamera.m_Zoom + inCamera.m_X,
        local.y() / inCamera.m_Zoom + inCamera.m_Y
    };
}

asge::math::Rect asge::video::TransformRect(
    Camera const &inCamera, math::Rect const &inWorldRect) noexcept
{
    math::Float2 const topLeft = WorldToScreen(inCamera, { inWorldRect.m_X, inWorldRect.m_Y });
    return math::Rect{
        topLeft.x(), topLeft.y(),
        inWorldRect.m_Width  * inCamera.m_Zoom,
        inWorldRect.m_Height * inCamera.m_Zoom
    };
}

#pragma once

#include <ASGE/ASGE.hpp>

class MovingBox
{
    asge::math::Float2 m_Position{ asge::math::Float2::Zero() };
    asge::math::Float2 m_Dimension{ 50.0f, 50.0f };

    float m_Speed{250.0f};
    bool  m_Up{false};
    bool  m_Down{false};
    bool  m_Left{false};
    bool  m_Right{false};
public:
    void Update(float inDeltaTime);
    void Render(asge::graphics::IRenderer& inRender);
    void OnKeyboardEvent(asge::event::KeyboardEvent const& inKeyEvent);
};
#include "MovingBox.hpp"

void MovingBox::Update(float inDeltaTime)
{
    auto dir = asge::math::Float2::Zero();

    if (m_Left)  dir.x() -= 1.0f;
    if (m_Right) dir.x() += 1.0f;
    if (m_Up)    dir.y() -= 1.0f;
    if (m_Down)  dir.y() += 1.0f;

    m_Position += dir * m_Speed * inDeltaTime;
}

void MovingBox::Render(asge::video::IRenderer &inRender)
{
    inRender.DrawRect(
        asge::math::Rect{ m_Position.x(), m_Position.y(), m_Dimension.x(), m_Dimension.y() },
        {255, 0, 0, 255},
        true
    );
}

void MovingBox::OnKeyboardEvent(asge::event::KeyboardEvent const &inKeyEvent)
{
    bool pressed = inKeyEvent.s_Type == asge::event::EventType::KEYBOARD_KEY_PRESSED;

    switch (inKeyEvent.s_Keycode)
    {
    case asge::input::Keycode::W: m_Up = pressed; break;
    case asge::input::Keycode::S: m_Down = pressed; break;
    case asge::input::Keycode::A: m_Left = pressed; break;
    case asge::input::Keycode::D: m_Right = pressed; break;
    default: break;
    }
}

#include "InputState.hpp"

using namespace asge::input;
using namespace asge::event;

void InputState::Consume(SystemEvent const &inEvent) noexcept
{
    if ( auto const* key = inEvent.TryGet<KeyboardEvent>() )
    {
        auto const idx = static_cast<std::size_t>( key->s_Keycode );
        if ( idx < kKeyCount ) m_KeyCurrentFrame[idx] = key->s_Down;
        return;
    }

    if ( auto const* motion = inEvent.TryGet<MouseMotionEvent>() )
    {
        m_MousePos = motion->s_Position;
        return;
    }

    if ( auto const* button = inEvent.TryGet<MouseButtonEvent>() )
    {
        auto const idx = static_cast<std::size_t>( button->s_Button );
        if ( idx < kMouseBCount ) m_MouseCurrentFrame[idx] = button->s_Down;
        return;
    }

    if ( auto const* wheel = inEvent.TryGet<MouseWheelEvent>() )
    {
        m_ScrollDelta += wheel->s_Scroll;
        return;
    }
}

void InputState::NewFrame() noexcept
{
    m_KeyPreviousFrame = m_KeyCurrentFrame;
    m_MousePreviousFrame = m_MouseCurrentFrame;
    m_PreviousMousePos = m_MousePos;
    m_ScrollDelta = math::Float2::Zero();
}

bool InputState::IsKeyDown(Keycode inKey) const noexcept
{
    return m_KeyCurrentFrame[static_cast<std::size_t>(inKey)];
}

bool InputState::IsKeyPressed(Keycode inKey) const noexcept
{
    auto const idx = static_cast<std::size_t>(inKey);
    return m_KeyCurrentFrame[idx] && !m_KeyPreviousFrame[idx];
}

bool InputState::IsKeyReleased(Keycode inKey) const noexcept
{
    auto const idx = static_cast<std::size_t>(inKey);
    return !m_KeyCurrentFrame[idx] && m_KeyPreviousFrame[idx];
}

bool InputState::IsMouseButtonDown(MouseButton inButton) const noexcept
{
    return m_MouseCurrentFrame[static_cast<std::size_t>(inButton)];
}

bool InputState::IsMouseButtonPressed(MouseButton inButton) const noexcept
{
    auto const idx = static_cast<std::size_t>(inButton);
    return m_MouseCurrentFrame[idx] && !m_MousePreviousFrame[idx];
}

bool InputState::IsMouseButtonReleased(MouseButton inButton) const noexcept
{
    auto const idx = static_cast<std::size_t>(inButton);
    return !m_MouseCurrentFrame[idx] && m_MousePreviousFrame[idx];
}

asge::math::Float2 InputState::GetMousePosition() const noexcept
{
    return m_MousePos;
}

asge::math::Float2 InputState::GetMouseDelta() const noexcept
{
    return m_MousePos - m_PreviousMousePos;
}

asge::math::Float2 InputState::GetScrollDelta() const noexcept
{
    return m_ScrollDelta;
}

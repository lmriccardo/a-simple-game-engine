#include "InputSystem.hpp"

using namespace asge::input;

void InputSystem::NewFrame() noexcept
{
    m_State.NewFrame();
}

void InputSystem::ProcessEvent(event::SystemEvent const &inEvent) noexcept
{
    m_State.Consume( inEvent );
}

InputState const &InputSystem::GetState() const noexcept
{
    return m_State;
}

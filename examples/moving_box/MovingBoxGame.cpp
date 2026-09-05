#include "MovingBoxGame.hpp"

std::optional<asge::game::state::Transition<int>>
MovingBoxState::Update(float inDeltaTime, [[maybe_unused]] asge::input::InputState const& inInput)
{
    m_Box.Update( inDeltaTime );
    return std::nullopt;
}

void MovingBoxState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 0, 0, 0, 255 });
    m_Box.Render( inRenderer );
}

void MovingBoxState::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>();
    if (!keyEvent) return;
    m_Box.OnKeyboardEvent( *keyEvent );
}

MovingBoxGame::MovingBoxGame(asge::video::IRenderer& inRenderer)
: Game(inRenderer)
{
    SetInitialState(0);
}

std::unique_ptr<MovingBoxGame::StateType> MovingBoxGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<MovingBoxState>();
}

#include "Game.hpp"

std::optional<asge::game::state::Transition<int>> BackgroundChangingState::Update(
    [[maybe_unused]] float inDeltaTime, [[maybe_unused]] asge::input::InputState const& inInput)
{
    return std::nullopt;
}

void BackgroundChangingState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear(m_BackgroundComponent.GetColor());
}

void BackgroundChangingState::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    if ( auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>() )
    {
        if ( keyEvent->s_Down && keyEvent->s_Keycode == asge::input::Keycode::SPACE )
        {
            m_BackgroundComponent.SetColor( BackgroundComponent::GetRandomColor() );
        }
    }
}

BackgroundChangingGame::BackgroundChangingGame(asge::video::IRenderer& inRenderer)
: Game(inRenderer)
{
    SetInitialState(0);
}

std::unique_ptr<BackgroundChangingGame::StateType>
BackgroundChangingGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<BackgroundChangingState>();
}

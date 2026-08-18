#include "Game.hpp"

void BackgroundChangingGame::Update([[maybe_unused]]float inDeltaTime)
{
}

void BackgroundChangingGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear(m_BackgroundComponent.GetColor());
}

void BackgroundChangingGame::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    if ( auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>() )
    {
        if ( keyEvent->s_Down && keyEvent->s_Keycode == asge::input::Keycode::SPACE )
        {
            m_BackgroundComponent.SetColor( BackgroundComponent::GetRandomColor() );
        }
    }

    return;
}

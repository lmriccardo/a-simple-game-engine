#include "MovingBoxGame.hpp"

void MovingBoxGame::Update(float inDeltaTime)
{
    m_Box.Update( inDeltaTime );
}

void MovingBoxGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 0, 0, 0, 255 });
    m_Box.Render( inRenderer );
}

void MovingBoxGame::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>();
    if (!keyEvent) return;
    m_Box.OnKeyboardEvent( *keyEvent );
}

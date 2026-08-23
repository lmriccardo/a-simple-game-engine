#pragma once

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Events/Events.hpp>
#include <ASGE/Input/InputState.hpp>

namespace asge::game
{

class IGame
{
public:
    virtual ~IGame() = default;
    virtual void Update(float inDeltaTime, input::InputState const& inInput) = 0;
    virtual void Render(video::IRenderer& inRenderer) = 0;
    virtual void OnSystemEvent(event::SystemEvent const& inSysEvent) = 0;
};

}
#pragma once

#include <ASGE/Graphics/Renderer.hpp>

namespace asge::game
{

class IGame
{
public:
    virtual ~IGame() = default;
    virtual void Update(float inDeltaTime) = 0;
    virtual void Render(graphics::IRenderer& inRenderer) = 0;
};

}
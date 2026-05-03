#pragma once

#include <ASGE/ASGE.hpp>
#include "MovingBox.hpp"

class MovingBoxGame : public asge::game::IGame
{
    MovingBox m_Box;
public:
    ~MovingBoxGame() override = default;

    void Update(float inDeltaTime) override;
    void Render(asge::graphics::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};
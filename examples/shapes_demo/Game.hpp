#pragma once

#include <ASGE/ASGE.hpp>

class ShapesDemoGame : public asge::game::IGame
{
    float m_LineAngle{0.0f}; // Drives the sweeping DrawLine demo

public:
    ~ShapesDemoGame() override = default;

    void Update(float inDeltaTime) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

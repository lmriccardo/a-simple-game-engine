#pragma once

#include <ASGE/ASGE.hpp>

class ShapesDemoState final : public asge::game::state::IGameState<int>
{
    float m_LineAngle{0.0f}; // Drives the sweeping DrawLine demo

public:
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class ShapesDemoGame final : public asge::game::Game<int>
{
public:
    explicit ShapesDemoGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

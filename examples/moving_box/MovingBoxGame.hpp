#pragma once

#include <ASGE/ASGE.hpp>
#include "MovingBox.hpp"

class MovingBoxState final : public asge::game::state::IGameState<int>
{
    MovingBox m_Box;

public:
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class MovingBoxGame final : public asge::game::Game<int>
{
public:
    explicit MovingBoxGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

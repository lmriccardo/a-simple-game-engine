#pragma once

#include <ASGE/ASGE.hpp>
#include "Component.hpp"

class BackgroundChangingState final : public asge::game::state::IGameState<int>
{
    BackgroundComponent m_BackgroundComponent{ BackgroundComponent::GetRandomColor() };

public:
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class BackgroundChangingGame final : public asge::game::Game<int>
{
public:
    explicit BackgroundChangingGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

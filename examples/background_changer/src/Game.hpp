#pragma once

#include <ASGE/ASGE.hpp>
#include "Component.hpp"

class BackgroundChangingGame : public asge::game::IGame
{
    BackgroundComponent m_BackgroundComponent{ BackgroundComponent::GetRandomColor() };
public:
    ~BackgroundChangingGame() override = default;

    void Update(float inDeltaTime);
    void Render(asge::graphics::IRenderer& inRenderer);
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent);
};
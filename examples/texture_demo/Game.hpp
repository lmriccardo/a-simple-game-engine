#pragma once

#include <ASGE/ASGE.hpp>

class TextureDemoState final : public asge::game::state::IGameState<int>
{
    std::unique_ptr<asge::video::ITexture> m_CheckerTexture; // Lazily created on first Render
    std::unique_ptr<asge::video::ITexture> m_FrameTexture;
    float m_RotationAngle{0.0f}; // Drives the DrawTextureAffine demo

    void EnsureTexturesLoaded(asge::video::IRenderer& inRenderer);

public:
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class TextureDemoGame final : public asge::game::Game<int>
{
public:
    explicit TextureDemoGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

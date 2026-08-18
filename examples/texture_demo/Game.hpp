#pragma once

#include <ASGE/ASGE.hpp>

class TextureDemoGame : public asge::game::IGame
{
    std::unique_ptr<asge::video::ITexture> m_CheckerTexture; // Lazily created on first Render (no IRenderer exists yet at construction)
    std::unique_ptr<asge::video::ITexture> m_FrameTexture;
    float m_RotationAngle{0.0f}; // Drives the DrawTextureAffine demo

    void EnsureTexturesLoaded(asge::video::IRenderer& inRenderer);

public:
    ~TextureDemoGame() override = default;

    void Update(float inDeltaTime) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

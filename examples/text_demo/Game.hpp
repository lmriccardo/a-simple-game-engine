#pragma once

#include <ASGE/ASGE.hpp>

class TextDemoGame : public asge::game::IGame
{
    // Each Font is baked at one fixed pixel height, so showing multiple
    // sizes on screen means loading the font multiple times -- once per
    // size -- each with its own atlas and uploaded texture.
    std::unique_ptr<asge::media::Font> m_SmallFont;
    std::unique_ptr<asge::media::Font> m_MediumFont;
    std::unique_ptr<asge::media::Font> m_LargeFont;
    std::unique_ptr<asge::video::ITexture> m_SmallAtlas;
    std::unique_ptr<asge::video::ITexture> m_MediumAtlas;
    std::unique_ptr<asge::video::ITexture> m_LargeAtlas;

    float m_HueTime{0.0f}; // drives the color-cycling headline

    void EnsureFontsLoaded(asge::video::IRenderer& inRenderer);

public:
    ~TextDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

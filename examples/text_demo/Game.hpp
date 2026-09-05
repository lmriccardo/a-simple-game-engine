#pragma once

#include <ASGE/ASGE.hpp>

class TextDemoState final : public asge::game::state::IGameState<int>
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
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class TextDemoGame final : public asge::game::Game<int>
{
public:
    explicit TextDemoGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

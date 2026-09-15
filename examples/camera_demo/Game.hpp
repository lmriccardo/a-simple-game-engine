#pragma once

#include <ASGE/ASGE.hpp>

/**
 * @brief Showcases IRenderer's Camera/Viewport pair: a large world panned
 *        and zoomed by a controllable Camera, plus a picture-in-picture
 *        minimap rendered through a second Camera into a small Viewport.
 */
class CameraDemoState final : public asge::game::state::IGameState<int>
{
    asge::video::Camera m_Camera{}; // world-space camera, panned/zoomed by input

    void UpdateCamera(float inDeltaTime, asge::input::InputState const& inInput);
    void RenderWorld(asge::video::IRenderer& inRenderer) const;
    void RenderMinimap(asge::video::IRenderer& inRenderer) const;

public:
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class CameraDemoGame final : public asge::game::Game<int>
{
public:
    explicit CameraDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

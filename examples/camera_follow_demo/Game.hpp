#pragma once

#include <ASGE/ASGE.hpp>
#include <vector>

/**
 * @brief Showcases components::Camera/resources::ActiveCamera/systems::
 *        CameraSystem: a player entity WASD-driven through a tiled world
 *        far bigger than the window, with the camera smoothly following it
 *        and systems::RenderSystem culling every tile outside the visible
 *        area rather than submitting a draw call for all of them.
 *
 * Unlike camera_demo (which drives asge::video::Camera directly, with no
 * ECS involved), this demo never touches IRenderer::SetCamera itself —
 * CameraSystem does that each frame from the ActiveCamera entity's own
 * Transform + Camera components, the same way a real game would.
 */
class CameraFollowDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    asge::ecs::Entity m_Player{ asge::ecs::Entity::Null() };
    std::unique_ptr<asge::video::ITexture> m_TileTexture; // Lazily created on first Render
    bool m_SpritesAttached{ false };

    bool m_Up{ false };
    bool m_Down{ false };
    bool m_Left{ false };
    bool m_Right{ false };
    float m_LastDeltaTime{ 0.0f }; // Captured in Update(), consumed by Render()'s RenderPipeline call

    void SpawnWorld();
    void EnsureSpritesAttached(asge::video::IRenderer& inRenderer);
    void UpdatePlayerVelocity();
    void AdjustZoom(float inDelta);
    void ToggleSmoothing();
    void RenderHud(asge::video::IRenderer& inRenderer) const;

public:
    CameraFollowDemoState(asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class CameraFollowDemoGame final : public asge::game::Game<int>
{
public:
    explicit CameraFollowDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

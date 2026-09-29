#pragma once

#include <ASGE/ASGE.hpp>

#include <memory>
#include <string>
#include <unordered_map>

/**
 * @brief Loads its whole entity set from a hand-authored TOML scene file
 * (assets/scene.toml) via SceneManager::LoadScene instead of spawning
 * entities in code — see ecs_demo for the code-driven equivalent this
 * replaces.
 *
 * Ties together every ASGE subsystem at its current state: VirtualFileSystem
 * + AssetManager resolve the scene file and each Sprite's texture by virtual
 * path; Registry/View run the ECS side; InputState is polled in Update()
 * (see input_demo) to drive the player entity (the scene's last one, by
 * convention — see scene.toml's own comments). P calls SceneManager::SaveScene
 * on the live, moved-around Registry — the other half of the round-trip —
 * and L requests a swap to a second scene file (assets/scene_alt.toml) via
 * SceneManager::RequestLoad, applied once per frame from a point nothing is
 * iterating the Registry, demonstrating on-the-fly scene swapping.
 * OnSystemEvent is left empty, same as input_demo, since polling covers
 * everything this demo needs.
 *
 * Movement, layout, propagation and drawing of the active scene all happen in
 * Game<TStateId>'s own per-frame pipeline (SceneManager's Registry holds only
 * the active scene), so this state only reacts to input and scene swaps.
 */
class SceneDemoState final : public asge::game::state::IGameState<int>
{
    asge::game::scene::SceneManager& m_SceneManager;
    asge::game::asset::AssetManager& m_Assets;

    asge::ecs::Entity m_Player{ asge::ecs::Entity::Null() };

    void UpdatePlayerVelocity( asge::input::InputState const& inInput );
    void WrapAroundScreen();
    void SaveSceneSnapshot() const;
    void RefreshPlayerReference(); // re-finds "the player" after any (re)load

public:
    SceneDemoState(
        asge::game::scene::SceneManager& inSceneManager, asge::game::asset::AssetManager& inAssets );

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    [[nodiscard]] asge::graphics::RGBA_Color ClearColor() const noexcept override { return { 15, 15, 20, 255 }; }
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class SceneDemoGame final : public asge::game::Game<int>
{
public:
    explicit SceneDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

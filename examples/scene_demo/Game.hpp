#pragma once

#include <ASGE/ASGE.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>

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
 * Notably absent: asge::game::systems::MovementSystem/RenderSystem. Both
 * take a bare Registry and have no notion of "active scene" — SceneManager
 * now keeps every resident scene's entities in the *same* Registry (see its
 * own doc comment), so calling those systems against GetRegistry() directly
 * would move/draw every resident scene at once, not just the active one.
 * MoveActiveEntities()/RenderActiveEntities() below are small local
 * stand-ins scoped to SceneManager::ActiveEntities() instead — teaching the
 * shared systems to be scene-aware for real is future work, not this demo's.
 */
class SceneDemoGame : public asge::game::IGame
{
    // m_Vfs must outlive m_Assets/m_SceneManager (both only borrow it) --
    // declared first so member init order guarantees that regardless of
    // ctor-list order.
    asge::filesystem::VirtualFileSystem m_Vfs;
    asge::game::asset::AssetManager     m_Assets{ m_Vfs };
    asge::game::scene::SceneManager     m_SceneManager{ m_Vfs };

    asge::ecs::Entity m_Player{ asge::ecs::Entity::Null() };

    // Textures are created lazily on first Render (no IRenderer exists yet
    // at construction) and cached by the virtual path each Sprite names, so
    // several entities sharing one texture only create it once -- and so a
    // scene swap's freshly-loaded sprites still resolve against the same
    // cache instead of recreating a texture already loaded once.
    std::unordered_map<std::string, std::unique_ptr<asge::video::ITexture>> m_Textures;

    void ResolveSpriteTextures( asge::video::IRenderer& inRenderer );
    void UpdatePlayerVelocity( asge::input::InputState const& inInput );
    void MoveActiveEntities( float inDeltaTime );   // MovementSystem, scoped to ActiveEntities()
    void RenderActiveEntities( asge::video::IRenderer& inRenderer ); // RenderSystem, likewise
    void WrapAroundScreen();
    void SaveSceneSnapshot() const;
    void RefreshPlayerReference(); // re-finds "the player" after any (re)load

public:
    SceneDemoGame();
    ~SceneDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

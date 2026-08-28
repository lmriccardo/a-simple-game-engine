#pragma once

#include <ASGE/ASGE.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneSerializer.hpp>

#include <memory>
#include <string>
#include <unordered_map>

/**
 * @brief Loads its whole entity set from a hand-authored TOML scene file
 * (assets/scene.toml) via SceneSerializer::Load instead of spawning entities
 * in code — see ecs_demo for the code-driven equivalent this replaces.
 *
 * Ties together every ASGE subsystem at its current state: VirtualFileSystem
 * + AssetManager resolve the scene file and each Sprite's texture by virtual
 * path; Registry/View/MovementSystem/RenderSystem run the ECS side; InputState
 * is polled in Update() (see input_demo) to drive the player entity (the
 * scene's last one, by convention — see scene.toml's own comments) and,
 * edge-triggered, to call SceneSerializer::Save on P — writing the live,
 * moved-around Registry back out, the other half of the round-trip.
 * OnSystemEvent is left empty, same as input_demo, since polling covers
 * everything this demo needs.
 */
class SceneDemoGame : public asge::game::IGame
{
    // m_Vfs must outlive m_Assets/m_SceneSerializer (both only borrow it) --
    // declared first so member init order guarantees that regardless of
    // ctor-list order.
    asge::filesystem::VirtualFileSystem m_Vfs;
    asge::game::asset::AssetManager     m_Assets{ m_Vfs };
    asge::game::scene::SceneSerializer  m_SceneSerializer{ m_Vfs };

    asge::ecs::Registry m_Registry;
    asge::ecs::Entity   m_Player{ asge::ecs::Entity::Null() };

    // Textures are created lazily on first Render (no IRenderer exists yet
    // at construction) and cached by the virtual path each Sprite names, so
    // several entities sharing one texture only create it once.
    std::unordered_map<std::string, std::unique_ptr<asge::video::ITexture>> m_Textures;
    bool m_TexturesResolved{ false };

    void ResolveSpriteTextures( asge::video::IRenderer& inRenderer );
    void UpdatePlayerVelocity( asge::input::InputState const& inInput );
    void WrapAroundScreen();
    void SaveSceneSnapshot() const;

public:
    SceneDemoGame();
    ~SceneDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

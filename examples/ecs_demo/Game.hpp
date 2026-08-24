#pragma once

#include <ASGE/ASGE.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <vector>

class EcsDemoGame : public asge::game::IGame
{
    asge::ecs::Registry             m_Registry;
    std::vector<asge::ecs::Entity>  m_SpriteEntities; // every entity that gets a Sprite once loaded
    asge::ecs::Entity               m_Player{ asge::ecs::Entity::Null() };

    // m_Vfs must outlive m_Assets (AssetManager only borrows it) -- declared
    // first so member init order guarantees that regardless of ctor-list order.
    asge::filesystem::VirtualFileSystem m_Vfs;
    asge::game::asset::AssetManager     m_Assets{ m_Vfs };

    std::unique_ptr<asge::video::ITexture> m_Texture; // Lazily created on first Render (no IRenderer exists yet at construction)
    bool m_SpritesAttached{ false };

    bool m_Up{ false };
    bool m_Down{ false };
    bool m_Left{ false };
    bool m_Right{ false };

    void SpawnEntities();
    void EnsureSpritesAttached(asge::video::IRenderer& inRenderer);
    void WrapAroundScreen();
    void UpdatePlayerVelocity();

public:
    EcsDemoGame();
    ~EcsDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

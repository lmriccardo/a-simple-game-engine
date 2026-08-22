#pragma once

#include <ASGE/ASGE.hpp>
#include <vector>

class EcsDemoGame : public asge::game::IGame
{
    asge::ecs::Registry             m_Registry;
    std::vector<asge::ecs::Entity>  m_SpriteEntities; // every entity that gets a Sprite once loaded
    asge::ecs::Entity               m_Player{ asge::ecs::Entity::Null() };

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

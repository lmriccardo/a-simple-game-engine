#pragma once

#include <ASGE/ASGE.hpp>
#include <vector>

class EcsDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    std::vector<asge::ecs::Entity>  m_SpriteEntities; // every entity that gets a Sprite once loaded
    asge::ecs::Entity               m_Player{ asge::ecs::Entity::Null() };

    std::unique_ptr<asge::video::ITexture> m_Texture; // Lazily created on first Render
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
    EcsDemoState(asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class EcsDemoGame final : public asge::game::Game<int>
{
public:
    explicit EcsDemoGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

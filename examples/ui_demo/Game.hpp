#pragma once

#include <ASGE/ASGE.hpp>

/**
 * @brief Showcases UIButton -- the only UI widget RenderSystem draws so far.
 *
 * Two buttons sit side by side. The left one carries no Sprite, so
 * RenderSystem draws it as a flat rect, switching between UIButton's
 * m_Color/m_HoverColor/m_PressedColor as the pointer moves over and clicks
 * it. The right one also carries a Sprite, so RenderSystem draws *that*
 * instead of the button rect (see RenderSystem.cpp's ShouldExclude) -- this
 * demo draws its own hover/press outline around it to show the click is
 * still tracked even though the sprite itself never changes color.
 *
 * Neither button has a font attached, so nothing renders through
 * IRenderer::DrawString here; clicks are logged to the console instead.
 * There's also no engine-side UI input system yet, so this demo computes
 * hover/held from InputState itself each frame and fires UIButton::m_OnClick
 * on the held-to-released-while-still-hovered transition, exactly as
 * UIButton's own doc comment describes.
 */
class UIDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    asge::ecs::Entity m_PlainButton { asge::ecs::Entity::Null() }; // no Sprite -- drawn as a colored rect
    asge::ecs::Entity m_SpriteButton{ asge::ecs::Entity::Null() }; // has a Sprite -- drawn as the texture instead

    void SpawnEntities();
    void UpdateButton( asge::ecs::Entity inEntity, asge::math::Float2 inMousePos, bool inLeftDown );
    void RenderSpriteButtonOutline( asge::video::IRenderer& inRenderer ) const;

public:
    UIDemoState(asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class UIDemoGame final : public asge::game::Game<int>
{
public:
    explicit UIDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

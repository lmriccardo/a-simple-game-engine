#pragma once

#include <ASGE/ASGE.hpp>

/**
 * @brief Showcases UIButton -- the only UI widget RenderSystem draws so far.
 *
 * Two buttons sit side by side up top. The left one carries no Sprite, so
 * RenderSystem draws it as a flat rect, switching between UIButton's
 * m_Color/m_HoverColor/m_PressedColor as the pointer moves over and clicks
 * it. The right one also carries a Sprite, so RenderSystem draws *that*
 * instead of the button rect (see RenderSystem.cpp's ShouldExclude) -- this
 * demo draws its own hover/press outline around it to show the click is
 * still tracked even though the sprite itself never changes color.
 *
 * Below them, a second pair overlaps: m_OverlapFront sits on a higher
 * RenderInfo::m_Layer directly on top of m_OverlapBack, which is offset
 * just far enough that part of it still pokes out from underneath.
 * UIButtonSystem walks resources::UIHitList back to front (see its own doc
 * comment), so clicking the overlap resolves to the front button, while
 * clicking the exposed sliver still resolves to the back one.
 *
 * None of the four buttons has a font attached, so nothing renders through
 * IRenderer::DrawString here; clicks are logged to the console instead.
 * Hover/held/m_OnClick are entirely engine-driven: SpawnEntities() sets the
 * resources::UIHitList resource once, RenderSystem rebuilds it every frame,
 * and Update() just calls systems::UIButtonSystem -- this state never
 * touches InputState's mouse queries or a button's rect itself.
 */
class UIDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    asge::ecs::Entity m_PlainButton { asge::ecs::Entity::Null() }; // no Sprite -- drawn as a colored rect
    asge::ecs::Entity m_SpriteButton{ asge::ecs::Entity::Null() }; // has a Sprite -- drawn as the texture instead
    asge::ecs::Entity m_OverlapFront{ asge::ecs::Entity::Null() }; // higher layer -- sits on top of m_OverlapBack
    asge::ecs::Entity m_OverlapBack { asge::ecs::Entity::Null() }; // lower layer, shifted -- partly exposed and still clickable

    void SpawnEntities();
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

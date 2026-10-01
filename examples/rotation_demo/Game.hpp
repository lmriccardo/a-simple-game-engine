#pragma once

#include <ASGE/ASGE.hpp>

/**
 * @brief Showcases Transform::m_Rotation actually being applied by
 *        RenderSystem: a plain rotating sprite (the whole-texture
 *        DrawTextureAffine path) alongside a rotating *animated* sprite (the
 *        source-rect DrawTextureAffine overload — rotated and cropped to
 *        its current animation frame at once).
 *
 * Both entities just accumulate their own angular speed into their own
 * Transform::m_Rotation each Update() — there's no engine-side "spin"
 * component, this is ordinary per-entity gameplay state living in the demo.
 * LEFT/RIGHT adjust the plain sprite's spin speed (and can reverse its
 * direction), SPACE pauses/resumes both, R resets everything.
 */
class RotationDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    asge::ecs::Entity m_Spinner{ asge::ecs::Entity::Null() };         // whole-texture rotation
    asge::ecs::Entity m_AnimatedSpinner{ asge::ecs::Entity::Null() }; // rotation + cropped animation frame

    float m_SpinnerSpeed{ 1.5f }; // radians/second; LEFT/RIGHT-adjustable, can go negative
    bool  m_Paused{ false };

    void SpawnEntities();
    void UpdateRotation(float inDeltaTime);
    void AdjustSpinnerSpeed(float inDelta);
    void Reset();
    void RenderHud(asge::video::IRenderer& inRenderer) const;

public:
    RotationDemoState(asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    [[nodiscard]] asge::graphics::RGBA_Color ClearColor() const noexcept override { return { 15, 18, 22, 255 }; }
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class RotationDemoGame final : public asge::game::Game<int>
{
public:
    explicit RotationDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

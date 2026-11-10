#pragma once

/**
 * @brief Showcase for components::Animator, asset::AnimationStateMachine,
 *        asset::AnimationClipSet and systems::AnimatorSystem.
 *
 * assets/player.animsm declares the flags, triggers, clips and states of the
 * machine (idle, run, attack, hurt) and the transitions between them.
 * assets/player.animset binds each clip name to a FrameTable file and a frame
 * duration; all four tables slice the same spritesheet. Each Animator only
 * stores the clip set path -- AssetManager::ResolveAssets loads the set, its
 * machine and the frame tables once, shared by every Animator pointing at it.
 *
 * Hold A/D to raise the "running" flag, press J to fire "attack" and K to
 * fire "hurt"; the machine picks the state. Space stops/resumes both
 * animators. The right sprite runs the same clip set at half playback speed.
 */

#include <ASGE/ASGE.hpp>
#include <vector>

class AnimatorDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;

    std::vector<asge::ecs::Entity> m_Entities; // every animated sprite
    bool m_Stopped{ false };

    void Spawn( asge::math::Float2 inPosition, float inPlaybackSpeed );
    void HandleInput( asge::input::InputState const& inInput );

public:
    AnimatorDemoState(asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    [[nodiscard]] asge::graphics::RGBA_Color ClearColor() const noexcept override { return { 15, 15, 20, 255 }; }
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class AnimatorDemoGame final : public asge::game::Game<int>
{
public:
    explicit AnimatorDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

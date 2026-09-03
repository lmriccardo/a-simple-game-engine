#pragma once

/**
 * @brief Showcase for components::Animation and systems::AnimationSystem/
 *        RenderPipeline: playing back a spritesheet through per-entity
 *        frame state.
 *
 * A 6-frame spritesheet (graphics::MakeGridFrames slices it into
 * components::Animation::m_Frames) drives a row of pre-spawned sprites, each
 * started on a different frame so they visibly play out of phase with each
 * other -- Animation is per-entity state, not shared playback. Left-click
 * spawns another animated sprite at the cursor, with its own randomized
 * frame duration and starting frame. Space toggles every sprite's Animation
 * between playing and paused (components::PlayAnimation/StopAnimation);
 * R resets back to the initial row.
 *
 * Every frame runs systems::RenderPipeline instead of calling AnimationSystem
 * and RenderSystem separately -- the single entry point a game loop actually
 * needs once it has animated sprites.
 */

#include <ASGE/ASGE.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <random>
#include <vector>

class AnimationDemoGame : public asge::game::IGame
{
    asge::ecs::Registry            m_Registry;
    std::vector<asge::ecs::Entity> m_SpriteEntities; // every entity spawned so far, in spawn order

    // m_Vfs must outlive m_Assets (AssetManager only borrows it) -- declared
    // first so member init order guarantees that regardless of ctor-list order.
    asge::filesystem::VirtualFileSystem m_Vfs;
    asge::game::asset::AssetManager     m_Assets{ m_Vfs };

    std::unique_ptr<asge::video::ITexture> m_Texture; // Lazily created on first Render (no IRenderer exists yet at construction)
    bool m_SpriteTextureAttached{ false };

    std::mt19937 m_Rng{ std::random_device{}() };
    bool m_Playing{ true };
    float m_LastDeltaTime{ 0.0f }; // Captured in Update(), consumed by Render()'s RenderPipeline call

    void SpawnInitialRow();
    void SpawnSprite( asge::math::Float2 inPosition, std::size_t inStartFrame );
    void EnsureTextureAttached( asge::video::IRenderer& inRenderer );
    void Reset();
    void HandleInput( asge::input::InputState const& inInput );

public:
    AnimationDemoGame();
    ~AnimationDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

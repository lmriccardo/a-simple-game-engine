#pragma once

/**
 * @brief Showcase for components::Animation and systems::AnimationSystem/
 *        RenderPipeline: playing back a spritesheet through per-entity
 *        frame state, with the frame list itself loaded as a shared
 *        asset::FrameTable rather than embedded per-entity.
 *
 * A 6-frame spritesheet, described by a small `walk.toml` FrameTable
 * meta-file (see assets/), drives a row of pre-spawned sprites, each
 * started on a different frame so they visibly play out of phase with each
 * other -- Animation is per-entity playback state even though every sprite
 * here shares the one loaded FrameTable asset. Left-click spawns another
 * animated sprite at the cursor, with its own randomized frame duration.
 * Space toggles every sprite's Animation between playing and paused
 * (components::PlayAnimation/StopAnimation); R resets back to the initial
 * row.
 *
 * Every Render() call runs asset::AssetManager::ResolveAssets first --
 * deferred-loading any still-unresolved Sprite::m_Texture/Animation::m_Clip
 * in one pass, entities spawned this frame included -- then
 * systems::RenderPipeline (AnimationSystem then RenderSystem). Spawning
 * itself just sets Sprite::m_VirtualPath/Animation::m_ClipPath; unlike
 * earlier examples (see ecs_demo), there's no hand-rolled "is the texture
 * attached yet" bookkeeping to write, since ResolveAssets already re-checks
 * per-entity every call.
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

    std::mt19937 m_Rng{ std::random_device{}() };
    bool m_Playing{ true };
    float m_LastDeltaTime{ 0.0f }; // Captured in Update(), consumed by Render()'s RenderPipeline call

    void SpawnInitialRow();
    void SpawnSprite( asge::math::Float2 inPosition, std::size_t inStartFrame );
    void Reset();
    void HandleInput( asge::input::InputState const& inInput );

public:
    AnimationDemoGame();
    ~AnimationDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

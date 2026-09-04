#include "Game.hpp"

namespace
{
using asge::game::components::Animation;
using asge::game::components::PlayAnimation;
using asge::game::components::Sprite;
using asge::game::components::StopAnimation;
using asge::game::components::Transform;

constexpr float        kWindowWidth      = 800.0f;
constexpr std::size_t  kFrameCount       = 6;   // must match assets/walk.toml's "count"
constexpr float        kCellSize         = 32.0f;
constexpr float        kSpriteScale      = 2.5f; // draws each 32x32 cell at 80x80 on screen
constexpr float        kRowY             = 250.0f;
constexpr float        kRowSpacing       = kWindowWidth / static_cast<float>(kFrameCount + 1);
constexpr float        kMinFrameDuration = 0.05f;
constexpr float        kMaxFrameDuration = 0.25f;

constexpr char const* kSheetPath = "textures/spritesheet.bmp";
constexpr char const* kClipPath  = "textures/walk.toml"; // asset::FrameTable meta-file -- see assets/
}

AnimationDemoGame::AnimationDemoGame()
{
    // ASGE_ANIMATION_DEMO_ASSET_DIR is injected by CMakeLists.txt; mounted
    // once so both the sheet and its FrameTable meta-file are loaded by
    // virtual path (see Render()'s ResolveAssets call) instead of a
    // hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("textures", ASGE_ANIMATION_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    SpawnInitialRow();
}

void AnimationDemoGame::SpawnInitialRow()
{
    // Every sprite plays the same clip, but each starts on a different
    // frame -- makes it visually obvious that Animation is per-entity
    // playback state, not one shared clock, even though every sprite here
    // shares the one loaded FrameTable asset (see Animation::m_Clip).
    for ( std::size_t ii = 0; ii < kFrameCount; ++ii )
    {
        float const x = kRowSpacing * static_cast<float>(ii + 1) - (kCellSize * kSpriteScale * 0.5f);
        SpawnSprite( asge::math::Float2{ x, kRowY }, ii );
    }
}

void AnimationDemoGame::SpawnSprite( asge::math::Float2 inPosition, std::size_t inStartFrame )
{
    auto entity = m_Registry.CreateEntity();
    if ( !entity ) { entity.LogError(); return; }

    m_Registry.AddComponent<Transform>( entity.Value(),
        Transform{ inPosition.x(), inPosition.y(), 0.0f, kSpriteScale, kSpriteScale } );

    std::uniform_real_distribution<float> durationDist( kMinFrameDuration, kMaxFrameDuration );
    m_Registry.AddComponent<Animation>( entity.Value(), Animation{
        .m_ClipPath = kClipPath,
        .m_FrameDuration = durationDist( m_Rng ),
        .m_CurrentFrame = inStartFrame,
        .m_Playing = m_Playing
    } );

    // m_Texture stays null and m_Clip (Animation, above) stays unresolved
    // until Render()'s AssetManager::ResolveAssets call -- no "is the
    // renderer ready yet" bookkeeping needed here, unlike earlier examples.
    m_Registry.AddComponent<Sprite>( entity.Value(), Sprite{ .m_VirtualPath = kSheetPath } );

    m_SpriteEntities.push_back( entity.Value() );
}

void AnimationDemoGame::Reset()
{
    for ( auto entity : m_SpriteEntities )
    {
        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }
    m_SpriteEntities.clear();
    m_Playing = true;
    SpawnInitialRow();
}

void AnimationDemoGame::HandleInput(asge::input::InputState const &inInput)
{
    using asge::input::Keycode;
    using asge::input::MouseButton;

    if ( inInput.IsMouseButtonPressed( MouseButton::LEFT ) )
    {
        auto const mouse = inInput.GetMousePosition();
        SpawnSprite(
            asge::math::Float2{ mouse.x() - kCellSize * kSpriteScale * 0.5f,
                                 mouse.y() - kCellSize * kSpriteScale * 0.5f },
            0 );
    }

    if ( inInput.IsKeyPressed( Keycode::SPACE ) )
    {
        m_Playing = !m_Playing;
        for ( auto entity : m_SpriteEntities )
        {
            auto result = m_Registry.GetComponent<Animation>( entity );
            if ( !result ) continue;
            if ( m_Playing ) PlayAnimation( result.Value().get() );
            else StopAnimation( result.Value().get() );
        }
    }

    if ( inInput.IsKeyPressed( Keycode::R ) )
    {
        Reset();
    }
}

void AnimationDemoGame::Update(float inDeltaTime, asge::input::InputState const &inInput)
{
    HandleInput( inInput );
    // AnimationSystem itself runs inside RenderPipeline (see Render()) --
    // Application::Run() calls Update() then Render() back-to-back once per
    // frame, so this frame's real inDeltaTime carries over unchanged.
    m_LastDeltaTime = inDeltaTime;
}

void AnimationDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 15, 15, 20, 255 });

    // Deferred-loads every Sprite::m_Texture/Animation::m_Clip still unset --
    // entities spawned this very frame (a fresh click) included, since it
    // re-checks the whole Registry rather than tracking "already resolved"
    // itself.
    m_Assets.ResolveAssets( m_Registry, inRenderer );

    // The one call this whole demo exists to show off: AnimationSystem
    // (advances every entity's Animation/Sprite::m_SourceRect) then
    // RenderSystem (draws), sequenced for the caller.
    asge::game::systems::RenderPipeline( m_Registry, inRenderer, m_LastDeltaTime );
}

void AnimationDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Everything here is driven by polling InputState in Update() instead.
}

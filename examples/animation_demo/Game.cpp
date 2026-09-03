#include "Game.hpp"

#include <algorithm>

namespace
{
using asge::game::components::Animation;
using asge::game::components::PlayAnimation;
using asge::game::components::Sprite;
using asge::game::components::StopAnimation;
using asge::game::components::Transform;

constexpr float        kWindowWidth      = 800.0f;
constexpr std::size_t  kColumns          = 6;   // spritesheet.bmp is 6 cells wide, 1 tall
constexpr std::size_t  kFrameCount       = 6;
constexpr float        kCellSize         = 32.0f;
constexpr float        kSpriteScale      = 2.5f; // draws each 32x32 cell at 80x80 on screen
constexpr float        kRowY             = 250.0f;
constexpr float        kRowSpacing       = kWindowWidth / static_cast<float>(kFrameCount + 1);
constexpr float        kMinFrameDuration = 0.05f;
constexpr float        kMaxFrameDuration = 0.25f;

// The frame rects themselves don't depend on the texture actually being
// loaded yet -- just the sheet's known grid layout -- so this can be
// computed once up front and shared by every Animation this demo spawns.
std::vector<asge::math::Rect> const& SheetFrames()
{
    static std::vector<asge::math::Rect> const frames = asge::graphics::MakeGridFrames(
        asge::math::Rect{ 0.0f, 0.0f, kCellSize, kCellSize }, kColumns, kFrameCount );
    return frames;
}
}

AnimationDemoGame::AnimationDemoGame()
{
    // ASGE_ANIMATION_DEMO_ASSET_DIR is injected by CMakeLists.txt; mounted
    // once so the sheet is loaded by virtual path (see EnsureTextureAttached)
    // instead of a hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("textures", ASGE_ANIMATION_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    SpawnInitialRow();
}

void AnimationDemoGame::SpawnInitialRow()
{
    // Every sprite plays the same 6-frame loop, but each starts on a
    // different frame -- makes it visually obvious that Animation is
    // per-entity playback state, not one shared clock.
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
    Animation anim{
        .m_Frames = SheetFrames(),
        .m_FrameDuration = durationDist( m_Rng ),
        .m_CurrentFrame = inStartFrame % std::max<std::size_t>(SheetFrames().size(), 1),
        .m_Playing = m_Playing
    };
    m_Registry.AddComponent<Animation>( entity.Value(), std::move(anim) );

    // The texture doesn't exist yet if this runs before the first Render()
    // call (the constructor's initial row) -- EnsureTextureAttached() adds
    // Sprite to every entity still missing one once it does.
    if ( m_Texture )
    {
        m_Registry.AddComponent<Sprite>( entity.Value(),
            Sprite{ m_Texture.get(), std::nullopt, "textures/spritesheet.bmp" } );
    }

    m_SpriteEntities.push_back( entity.Value() );
}

void AnimationDemoGame::EnsureTextureAttached(asge::video::IRenderer &inRenderer)
{
    if ( m_SpriteTextureAttached ) return;

    constexpr char const* kSheetPath = "textures/spritesheet.bmp";
    auto imageAsset = m_Assets.GetImage(kSheetPath);
    if ( !imageAsset )
    {
        imageAsset.LogError();
        return;
    }

    m_Texture = inRenderer.CreateTexture(imageAsset.Value()->Get());
    if ( !m_Texture ) return;

    for ( auto entity : m_SpriteEntities )
    {
        if ( m_Registry.HasComponent<Sprite>( entity ) ) continue;
        auto result = m_Registry.AddComponent<Sprite>(
            entity, Sprite{ m_Texture.get(), std::nullopt, kSheetPath } );
        if ( !result ) result.LogError();
    }

    m_SpriteTextureAttached = true;
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
    EnsureTextureAttached(inRenderer);

    // The one call this whole demo exists to show off: AnimationSystem
    // (advances every entity's Animation/Sprite::m_SourceRect) then
    // RenderSystem (draws), sequenced for the caller.
    asge::game::systems::RenderPipeline( m_Registry, inRenderer, m_LastDeltaTime );
}

void AnimationDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Everything here is driven by polling InputState in Update() instead.
}

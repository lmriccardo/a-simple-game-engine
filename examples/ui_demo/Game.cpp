#include "Game.hpp"

namespace
{
using asge::game::components::RenderInfo;
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::UIButton;
using asge::game::resources::UIHitList;

constexpr char const* kSpriteTexturePath = "textures/checker.bmp";

// asge::math::Float2 has no constexpr constructor (see Vec2's variadic ctor),
// so positions/sizes are kept as plain floats and assembled at point of use.
constexpr float kPlainButtonX = 220.0f, kPlainButtonY = 275.0f;
constexpr float kPlainButtonW = 160.0f, kPlainButtonH = 50.0f;

constexpr float kSpriteButtonX = 480.0f, kSpriteButtonY = 275.0f;
constexpr float kSpriteButtonScale = 5.0f; // checker.bmp is 32x32, scaled 5x -- 160x160 on screen
constexpr float kSpriteTextureSize = 32.0f;

constexpr float kOverlapW = 160.0f, kOverlapH = 50.0f;
constexpr float kOverlapFrontX = 220.0f, kOverlapFrontY = 420.0f;
// Shifted just enough that a kOverlapShift-wide/tall sliver along the
// bottom and right edges stays outside m_OverlapFront's rect -- click there.
constexpr float kOverlapShift = 30.0f;
constexpr float kOverlapBackX = kOverlapFrontX + kOverlapShift, kOverlapBackY = kOverlapFrontY + kOverlapShift;
}

UIDemoState::UIDemoState(
    asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets
)
: m_Registry(inRegistry), m_Assets(inAssets)
{
    SpawnEntities();
}

void UIDemoState::SpawnEntities()
{
    // Opts this registry into hit-testing at all -- RenderSystem only
    // rebuilds UIHitList once this resource exists (see its own doc
    // comment), and UIButtonSystem no-ops entirely without it.
    m_Registry.SetResource( UIHitList{} );

    // Both buttons draw in screen space -- ordinary UI convention, and what
    // keeps their position pixel-stable regardless of any world camera.
    auto plain = m_Registry.CreateEntity();
    if ( !plain ) { plain.LogError(); return; }
    m_PlainButton = plain.Value();
    m_Registry.AddComponent<Transform>( m_PlainButton, Transform{
        .m_WorldCoordinates = { kPlainButtonX, kPlainButtonY }
    } );
    m_Registry.AddComponent<UIButton>( m_PlainButton, UIButton{
        .m_Size = { kPlainButtonW, kPlainButtonH }
    } );
    m_Registry.AddComponent<RenderInfo>( m_PlainButton, RenderInfo{ .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UIButton>( m_PlainButton ) )
    {
        result.Value().get().m_OnClick.Connect( []{ LOG_INFO( "Plain button clicked!" ); } );
    }

    // Also carries a Sprite -- RenderSystem's ShouldExclude<UIButton> then
    // skips this entity's button rect entirely and draws the Sprite instead
    // (see RenderSystem.cpp), so this one's own size/color fields are never
    // read for drawing; its footprint comes from the texture and scale below.
    auto sprited = m_Registry.CreateEntity();
    if ( !sprited ) { sprited.LogError(); return; }
    m_SpriteButton = sprited.Value();
    m_Registry.AddComponent<Transform>( m_SpriteButton, Transform{
        .m_WorldCoordinates = { kSpriteButtonX, kSpriteButtonY },
        .m_WorldScale = { kSpriteButtonScale, kSpriteButtonScale }
    } );
    // m_Texture stays null until Render()'s AssetManager::ResolveAssets call.
    m_Registry.AddComponent<Sprite>( m_SpriteButton, Sprite{ .m_VirtualPath = kSpriteTexturePath } );
    m_Registry.AddComponent<UIButton>( m_SpriteButton, UIButton{
        .m_Size = { kSpriteButtonScale * kSpriteTextureSize, kSpriteButtonScale * kSpriteTextureSize }
    } );
    m_Registry.AddComponent<RenderInfo>( m_SpriteButton, RenderInfo{ .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UIButton>( m_SpriteButton ) )
    {
        result.Value().get().m_OnClick.Connect( []{ LOG_INFO( "Sprite button clicked!" ); } );
    }

    // m_OverlapBack first, so it also ends up behind in entity-index order --
    // m_Layer is what actually decides draw/hit order here, though, not this.
    auto back = m_Registry.CreateEntity();
    if ( !back ) { back.LogError(); return; }
    m_OverlapBack = back.Value();
    m_Registry.AddComponent<Transform>( m_OverlapBack, Transform{
        .m_WorldCoordinates = { kOverlapBackX, kOverlapBackY }
    } );
    m_Registry.AddComponent<UIButton>( m_OverlapBack, UIButton{
        .m_Color = asge::graphics::colors::s_ButtonDark,
        .m_HoverColor = asge::graphics::colors::s_ButtonDarkHover,
        .m_PressedColor = asge::graphics::colors::s_ButtonDarkPressed,
        .m_Size = { kOverlapW, kOverlapH }
    } );
    m_Registry.AddComponent<RenderInfo>( m_OverlapBack, RenderInfo{ .m_Layer = 0, .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UIButton>( m_OverlapBack ) )
    {
        result.Value().get().m_OnClick.Connect( []{ LOG_INFO( "Back button (B2) clicked!" ); } );
    }

    auto front = m_Registry.CreateEntity();
    if ( !front ) { front.LogError(); return; }
    m_OverlapFront = front.Value();
    m_Registry.AddComponent<Transform>( m_OverlapFront, Transform{
        .m_WorldCoordinates = { kOverlapFrontX, kOverlapFrontY }
    } );
    m_Registry.AddComponent<UIButton>( m_OverlapFront, UIButton{
        .m_Color = asge::graphics::colors::s_ButtonAccent,
        .m_HoverColor = asge::graphics::colors::s_ButtonAccentHover,
        .m_PressedColor = asge::graphics::colors::s_ButtonAccentPressed,
        .m_Size = { kOverlapW, kOverlapH }
    } );
    // Higher layer than m_OverlapBack -- both draws *and* hit-tests on top of
    // it (RenderSystem sorts by layer, CollectHitList keeps that same order,
    // and UIButtonSystem walks the hit list back to front).
    m_Registry.AddComponent<RenderInfo>( m_OverlapFront, RenderInfo{ .m_Layer = 1, .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UIButton>( m_OverlapFront ) )
    {
        result.Value().get().m_OnClick.Connect( []{ LOG_INFO( "Front button (B1) clicked!" ); } );
    }
}

void UIDemoState::RenderSpriteButtonOutline( asge::video::IRenderer &inRenderer ) const
{
    auto tResult = m_Registry.GetComponent<Transform>( m_SpriteButton );
    auto bResult = m_Registry.GetComponent<UIButton>( m_SpriteButton );
    if ( !tResult || !bResult ) return;

    auto const& transform = tResult.Value().get();
    auto const& button = bResult.Value().get();

    // RenderSystem never draws this button's own colors (see SpawnEntities),
    // so this is the only visible sign it's interactive at all.
    asge::graphics::RGBA_Color const outline = button.m_Held    ? asge::graphics::colors::s_ButtonAccentPressed
                                              : button.m_Hovered ? asge::graphics::colors::s_ButtonAccentHover
                                                                  : asge::graphics::colors::s_Gray;

    asge::math::Rect const rect{
        transform.m_WorldCoordinates.x() - 4.0f, transform.m_WorldCoordinates.y() - 4.0f,
        button.m_Size.x() + 8.0f, button.m_Size.y() + 8.0f
    };
    inRenderer.DrawRect( rect, outline, false );
}

std::optional<asge::game::state::Transition<int>>
UIDemoState::Update([[maybe_unused]] float inDeltaTime, asge::input::InputState const &inInput)
{
    // Resolves against last frame's UIHitList (see RenderSystem.cpp's
    // CollectHitList) -- both buttons are screen space, so the identity
    // camera here is fine even though this demo never sets one of its own.
    asge::game::systems::UIButtonSystem( m_Registry, inInput, asge::video::Camera{} );

    return std::nullopt;
}

void UIDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 24, 26, 30, 255 });

    // Deferred-loads the sprite button's Sprite::m_Texture on first use.
    m_Assets.ResolveAssets( m_Registry, inRenderer );

    asge::game::systems::RenderPipeline( m_Registry, inRenderer, 0.0f );

    RenderSpriteButtonOutline( inRenderer );
}

void UIDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
}

UIDemoGame::UIDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    // ASGE_UI_DEMO_ASSET_DIR is injected by CMakeLists.txt; mounted once so
    // the sprite button's texture loads by virtual path rather than a
    // hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("textures", ASGE_UI_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    SetInitialState(0);
}

std::unique_ptr<UIDemoGame::StateType> UIDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<UIDemoState>( m_SceneManager.GetRegistry(), m_Assets );
}

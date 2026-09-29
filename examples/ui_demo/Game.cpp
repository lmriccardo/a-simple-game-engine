#include "Game.hpp"

#include <ASGE/Game/UI.hpp>

namespace
{
using asge::game::components::Interactable;
using asge::game::components::RenderInfo;
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::UIButton;
using asge::game::components::UICheckbox;
using asge::game::components::UIRect;
using asge::game::resources::UIHitList;
using asge::game::ui::ButtonDesc;
using asge::game::ui::CreateButton;
using asge::game::ui::CreateLabel;
using asge::game::ui::CreateSlider;
using asge::game::ui::LabelDesc;
using asge::game::ui::SliderDesc;
using asge::game::ui::TextDesc;

constexpr char const* kSpriteTexturePath = "textures/checker.bmp";
constexpr char const* kFontPath = "fonts/PTSans-Regular.ttf";

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

// Third row: built through asge::game::ui's CreateLabel/CreateButton
// instead of hand-wiring components one at a time, unlike everything above.
constexpr float kRow3Y = 530.0f;
constexpr float kAutoLabelX = 60.0f;

constexpr float kFittingButtonX = 320.0f;
constexpr float kFittingButtonW = 150.0f, kFittingButtonH = 40.0f;

constexpr float kCroppedButtonX = 560.0f;
constexpr float kCroppedButtonW = 70.0f, kCroppedButtonH = 24.0f; // too small for its own label

// Fourth row: a hand-wired "Checkbox" -- UICheckbox has no asge::game::ui
// factory yet, unlike the label/button pair above.
constexpr float kRow4Y = 620.0f;
constexpr float kCheckboxX = 220.0f;
constexpr float kCheckboxSize = 24.0f;
constexpr float kCheckboxLabelX = kCheckboxX + kCheckboxSize + 12.0f;

// Fifth row: a slider built through asge::game::ui::CreateSlider.
constexpr float kRow5Y = 670.0f;
constexpr float kSliderX = 220.0f;
constexpr float kSliderW = 200.0f, kSliderH = 24.0f;
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
    // comment), and UIInteractionSystem no-ops entirely without it.
    m_Registry.SetResource( UIHitList{} );

    // Both buttons draw in screen space -- ordinary UI convention, and what
    // keeps their position pixel-stable regardless of any world camera.
    auto plain = m_Registry.CreateEntity();
    if ( !plain ) { plain.LogError(); return; }
    m_PlainButton = plain.Value();
    m_Registry.AddComponent<Transform>( m_PlainButton, Transform{
        .m_WorldCoordinates = { kPlainButtonX, kPlainButtonY }
    } );
    m_Registry.AddComponent<UIRect>( m_PlainButton, UIRect{ .m_Size = { kPlainButtonW, kPlainButtonH } } );
    m_Registry.AddComponent<UIButton>( m_PlainButton, UIButton{} );
    m_Registry.AddComponent<Interactable>( m_PlainButton, Interactable{} );
    m_Registry.AddComponent<RenderInfo>( m_PlainButton, RenderInfo{ .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UIButton>( m_PlainButton ) )
    {
        result.Value().get().m_OnClick.Connect( []{ LOG_INFO( "Plain button clicked!" ); } );
    }

    // Also carries a Sprite -- RenderSystem's ShouldExclude<UIRect> then
    // skips this entity's button rect entirely and draws the Sprite instead
    // (see RenderSystem.cpp), so UIRect's own size is never read for
    // drawing here; its footprint comes from the texture and scale below.
    // It's still hit-tested (CollectHitList only needs UIRect+Interactable,
    // not what actually got drawn), which is what RenderSpriteButtonOutline
    // relies on below. UIRect::m_Size is in the same *pre-scale* units as a
    // Sprite's native texture size (see RectFromSize/SpriteGetDstRect, both
    // of which multiply by Transform::m_WorldScale themselves) -- passing
    // the already-scaled 160x160 here double-applies m_WorldScale and blows
    // the hit rect out to 800x800.
    auto sprited = m_Registry.CreateEntity();
    if ( !sprited ) { sprited.LogError(); return; }
    m_SpriteButton = sprited.Value();
    m_Registry.AddComponent<Transform>( m_SpriteButton, Transform{
        .m_WorldCoordinates = { kSpriteButtonX, kSpriteButtonY },
        .m_WorldScale = { kSpriteButtonScale, kSpriteButtonScale }
    } );
    // m_Texture stays null until Render()'s AssetManager::ResolveAssets call.
    m_Registry.AddComponent<Sprite>( m_SpriteButton, Sprite{ .m_VirtualPath = kSpriteTexturePath } );
    m_Registry.AddComponent<UIRect>( m_SpriteButton, UIRect{
        .m_Size = { kSpriteTextureSize, kSpriteTextureSize }
    } );
    m_Registry.AddComponent<UIButton>( m_SpriteButton, UIButton{} );
    m_Registry.AddComponent<Interactable>( m_SpriteButton, Interactable{} );
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
    m_Registry.AddComponent<UIRect>( m_OverlapBack, UIRect{ .m_Size = { kOverlapW, kOverlapH } } );
    m_Registry.AddComponent<UIButton>( m_OverlapBack, UIButton{ .m_Colors = {
        .m_Color = asge::graphics::colors::s_ButtonDark,
        .m_HoverColor = asge::graphics::colors::s_ButtonDarkHover,
        .m_PressedColor = asge::graphics::colors::s_ButtonDarkPressed,
    } } );
    m_Registry.AddComponent<Interactable>( m_OverlapBack, Interactable{} );
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
    m_Registry.AddComponent<UIRect>( m_OverlapFront, UIRect{ .m_Size = { kOverlapW, kOverlapH } } );
    m_Registry.AddComponent<UIButton>( m_OverlapFront, UIButton{ .m_Colors = {
        .m_Color = asge::graphics::colors::s_ButtonAccent,
        .m_HoverColor = asge::graphics::colors::s_ButtonAccentHover,
        .m_PressedColor = asge::graphics::colors::s_ButtonAccentPressed,
    } } );
    m_Registry.AddComponent<Interactable>( m_OverlapFront, Interactable{} );
    // Higher layer than m_OverlapBack -- both draws *and* hit-tests on top of
    // it (RenderSystem sorts by layer, CollectHitList keeps that same order,
    // and UIInteractionSystem walks the hit list back to front).
    m_Registry.AddComponent<RenderInfo>( m_OverlapFront, RenderInfo{ .m_Layer = 1, .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UIButton>( m_OverlapFront ) )
    {
        result.Value().get().m_OnClick.Connect( []{ LOG_INFO( "Front button (B1) clicked!" ); } );
    }

    // A standalone label with no explicit UIRect size -- asge::game::ui::
    // LabelDesc::m_Size left as nullopt means "fit the text" (UILabel::
    // m_AutoSize = true), the auto-sizing counterpart to the two buttons
    // below, whose labels always take the button's own fixed size instead.
    auto autoLabel = CreateLabel( m_Registry, LabelDesc{
        .m_Name = "AutoLabel",
        .m_Position = { kAutoLabelX, kRow3Y },
        .m_Text = TextDesc{
            .m_Content = "Autosizable label!",
            .m_FontPath = kFontPath,
            .m_Color = asge::graphics::colors::s_White,
        },
    } );
    if ( !autoLabel ) autoLabel.LogError();

    // Sized generously for its short caption -- the label fits comfortably
    // inside the button with room to spare.
    auto fittingButton = CreateButton( m_Registry, ButtonDesc{
        .m_Name = "FittingButton",
        .m_Position = { kFittingButtonX, kRow3Y - 5.0f },
        .m_Size = { kFittingButtonW, kFittingButtonH },
        .m_Text = TextDesc{ .m_Content = "OK", .m_FontPath = kFontPath, .m_Align = asge::str::TextAlign::Center, .m_Color = asge::graphics::colors::s_Black },
        .m_OnClick = []{ LOG_INFO( "Fitting button clicked!" ); },
    } );
    if ( !fittingButton ) fittingButton.LogError();

    // A small button with a long caption -- button labels never auto-size
    // (see asge::game::ui::CreateButton), and RenderSystem's DrawString has
    // no clipping yet, so the text simply overflows past the button's edges
    // instead of being cut off at them.
    auto croppedButton = CreateButton( m_Registry, ButtonDesc{
        .m_Name = "CroppedButton",
        .m_Position = { kCroppedButtonX, kRow3Y },
        .m_Size = { kCroppedButtonW, kCroppedButtonH },
        .m_Text = TextDesc{
            .m_Content = "Way too long for this button",
            .m_FontPath = kFontPath,
            .m_FontPixelHeight = 14,
            .m_Color = asge::graphics::colors::s_Black,
        },
        .m_OnClick = []{ LOG_INFO( "Cropped button clicked!" ); },
    } );
    if ( !croppedButton ) croppedButton.LogError();

    // Hand-wired like the first four buttons -- UICheckbox + Interactable +
    // UIRect, no asge::game::ui factory for it yet.
    auto checkbox = m_Registry.CreateEntity();
    if ( !checkbox ) { checkbox.LogError(); return; }
    m_Checkbox = checkbox.Value();
    m_Registry.AddComponent<Transform>( m_Checkbox, Transform{
        .m_WorldCoordinates = { kCheckboxX, kRow4Y }
    } );
    m_Registry.AddComponent<UIRect>( m_Checkbox, UIRect{ .m_Size = { kCheckboxSize, kCheckboxSize } } );
    m_Registry.AddComponent<UICheckbox>( m_Checkbox, UICheckbox{} );
    m_Registry.AddComponent<Interactable>( m_Checkbox, Interactable{} );
    m_Registry.AddComponent<RenderInfo>( m_Checkbox, RenderInfo{ .m_ScreenSpace = true } );

    if ( auto result = m_Registry.GetComponent<UICheckbox>( m_Checkbox ) )
    {
        result.Value().get().m_OnToggled.Connect( []( bool inChecked )
        {
            LOG_INFO( "Checkbox toggled: {}", inChecked ? "checked" : "unchecked" );
        } );
    }

    // Caption beside it -- auto-sized, same as the standalone label in row 3.
    auto checkboxLabel = CreateLabel( m_Registry, LabelDesc{
        .m_Name = "CheckboxLabel",
        .m_Position = { kCheckboxLabelX, kRow4Y },
        .m_Text = TextDesc{
            .m_Content = "Enable something",
            .m_FontPath = kFontPath,
            .m_Color = asge::graphics::colors::s_White,
        },
    } );
    if ( !checkboxLabel ) checkboxLabel.LogError();

    // Fifth row: a slider through CreateSlider, starting at 0.4 of 0..1.
    auto slider = CreateSlider( m_Registry, SliderDesc{
        .m_Name = "Slider",
        .m_Position = { kSliderX, kRow5Y },
        .m_Size = { kSliderW, kSliderH },
        .m_Value = 0.4f,
        .m_OnValueChanged = []( float inValue ){ LOG_INFO( "Slider value: {:.2f}", inValue ); },
    } );
    if ( !slider ) slider.LogError();
    else m_Slider = slider.Value();
}

void UIDemoState::RenderSpriteButtonOutline( asge::video::IRenderer &inRenderer ) const
{
    auto tResult = m_Registry.GetComponent<Transform>( m_SpriteButton );
    auto rResult = m_Registry.GetComponent<UIRect>( m_SpriteButton );
    auto iResult = m_Registry.GetComponent<Interactable>( m_SpriteButton );
    if ( !tResult || !rResult || !iResult ) return;

    auto const& transform = tResult.Value().get();
    auto const& rect = rResult.Value().get();
    auto const& interactable = iResult.Value().get();

    // RenderSystem never draws this button's own colors (see SpawnEntities),
    // so this is the only visible sign it's interactive at all.
    asge::graphics::RGBA_Color const outline = interactable.m_Held    ? asge::graphics::colors::s_ButtonAccentPressed
                                              : interactable.m_Hovered ? asge::graphics::colors::s_ButtonAccentHover
                                                                       : asge::graphics::colors::s_Gray;

    // rect.m_Size is native (pre-scale) size -- scale it up the same way
    // RectFromSize/SpriteGetDstRect do, so the outline actually hugs the
    // sprite's real on-screen footprint instead of its unscaled one.
    float const width  = rect.m_Size.x() * transform.m_WorldScale.x();
    float const height = rect.m_Size.y() * transform.m_WorldScale.y();
    asge::math::Rect const outlineRect{
        transform.m_WorldCoordinates.x() - 4.0f, transform.m_WorldCoordinates.y() - 4.0f,
        width + 8.0f, height + 8.0f
    };
    inRenderer.DrawRect( outlineRect, outline, false );
}

std::optional<asge::game::state::Transition<int>>
UIDemoState::Update([[maybe_unused]] float inDeltaTime, asge::input::InputState const &inInput)
{
    // Resolves against last frame's UIHitList (see RenderSystem.cpp's
    // CollectHitList) -- both buttons are screen space, so the identity
    // camera here is fine even though this demo never sets one of its own.
    asge::game::systems::UIInteractionSystem( m_Registry, inInput, asge::video::Camera{} );

    return std::nullopt;
}

void UIDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 24, 26, 30, 255 });

    // Deferred-loads the sprite button's Sprite::m_Texture on first use.
    m_Assets.ResolveAssets( m_Registry, inRenderer );

    // The four hand-wired buttons above set Transform::m_WorldCoordinates
    // directly, but asge::game::ui::CreateLabel/CreateButton (the third row)
    // only set m_LocalCoordinates (+ m_Dirty) -- ordinary Transform usage,
    // meant to be flattened into m_WorldCoordinates by this system, which
    // RenderPipeline itself doesn't call.
    asge::game::systems::TransformPropagationSystem( m_Registry );

    asge::game::systems::RenderPipeline( m_Registry, inRenderer, 0.0f );

    RenderSpriteButtonOutline( inRenderer );
}

void UIDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
}

UIDemoGame::UIDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    // ASGE_UI_DEMO_ASSET_DIR is injected by CMakeLists.txt; mounted twice
    // (same directory, two virtual prefixes) so the sprite button's texture
    // and the labels' font both load by virtual path rather than a
    // hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("textures", ASGE_UI_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();
    auto fontsMountResult = m_Vfs.Mount("fonts", ASGE_UI_DEMO_ASSET_DIR);
    if ( !fontsMountResult ) fontsMountResult.LogError();

    SetInitialState(0);
}

std::unique_ptr<UIDemoGame::StateType> UIDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<UIDemoState>( m_SceneManager.GetRegistry(), m_Assets );
}

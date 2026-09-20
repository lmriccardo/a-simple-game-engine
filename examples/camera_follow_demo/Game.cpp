#include "Game.hpp"

#include <algorithm>
#include <array>

namespace
{
using asge::input::Keycode;
using asge::media::RGBA_Color;
using asge::game::components::Camera;
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::Velocity;
using asge::game::resources::ActiveCamera;

constexpr char const* kTilePath = "textures/checker.bmp";

constexpr float kTileSize  = 32.0f;
constexpr int   kGridCols  = 40;
constexpr int   kGridRows  = 30;
constexpr float kWorldW    = kGridCols * kTileSize; // 1280 -- comfortably bigger than the 800x600 window
constexpr float kWorldH    = kGridRows * kTileSize; // 960

constexpr float kPlayerSpeed  = 260.0f;
constexpr float kPlayerSize   = 40.0f;

constexpr float kZoomMin  = 0.4f;
constexpr float kZoomMax  = 2.5f;
constexpr float kZoomStep = 0.1f;
constexpr float kSmoothingRate = 4.0f; // catch-up rate when smoothing is toggled on -- see Camera::m_Smoothing

struct Landmark { asge::math::Float2 m_Pos; RGBA_Color m_Color; };

std::array<Landmark, 6> const kLandmarks{ {
    { {  100.0f,  100.0f }, RGBA_Color{ 220,  60,  60, 255 } },
    { { 1180.0f,  100.0f }, RGBA_Color{  60, 200,  90, 255 } },
    { {  640.0f,  480.0f }, RGBA_Color{ 240, 200,  60, 255 } },
    { {  100.0f,  860.0f }, RGBA_Color{  90, 170, 240, 255 } },
    { { 1180.0f,  860.0f }, RGBA_Color{ 200,  90, 220, 255 } },
    { {  640.0f,  100.0f }, RGBA_Color{ 240, 140,  60, 255 } },
} };
}

CameraFollowDemoState::CameraFollowDemoState(
    asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets
)
: m_Registry(inRegistry), m_Assets(inAssets)
{
    SpawnWorld();
}

void CameraFollowDemoState::SpawnWorld()
{
    // The tiled floor: one entity per grid cell, all sharing the one
    // checker texture once EnsureSpritesAttached runs -- RenderSystem culls
    // the ones outside the camera's current visible area (see
    // video::VisibleWorldRect), so only a fraction of these kGridCols *
    // kGridRows entities actually reach a draw call most frames.
    for ( int row = 0; row < kGridRows; ++row )
    {
        for ( int col = 0; col < kGridCols; ++col )
        {
            auto tile = m_Registry.CreateEntity();
            if ( !tile ) { tile.LogError(); continue; }

            asge::math::Float2 const tilePos{ static_cast<float>(col) * kTileSize, static_cast<float>(row) * kTileSize };
            m_Registry.AddComponent<Transform>( tile.Value(),
                Transform{ .m_LocalCoordinates = tilePos, .m_WorldCoordinates = tilePos } );
        }
    }

    // The player: Transform (its Camera target) + Velocity (WASD-driven,
    // via MovementSystem) + Camera (this is what makes it a valid
    // ActiveCamera target -- see resources::ActiveCamera's doc comment).
    auto player = m_Registry.CreateEntity();
    if ( !player ) { player.LogError(); return; }

    m_Player = player.Value();
    m_Registry.AddComponent<Transform>( m_Player, Transform{
        .m_LocalCoordinates = {kWorldW * 0.5f, kWorldH * 0.5f},
        .m_LocalScale = {kPlayerSize / kTileSize, kPlayerSize / kTileSize},
        .m_WorldCoordinates = {kWorldW * 0.5f, kWorldH * 0.5f},
        .m_WorldScale = {kPlayerSize / kTileSize, kPlayerSize / kTileSize}
    } );
    m_Registry.AddComponent<Velocity>( m_Player, Velocity{} );
    m_Registry.AddComponent<Camera>( m_Player, Camera{ .m_Zoom = 1.0f, .m_Smoothing = kSmoothingRate } );

    m_Registry.SetResource( ActiveCamera{ m_Player } );
}

void CameraFollowDemoState::EnsureSpritesAttached(asge::video::IRenderer &inRenderer)
{
    if ( m_SpritesAttached ) return;

    auto imageAsset = m_Assets.GetImage(kTilePath);
    if ( !imageAsset ) { imageAsset.LogError(); return; }

    m_TileTexture = inRenderer.CreateTexture(imageAsset.Value()->Get());
    if ( !m_TileTexture ) return;

    // Every entity with a Transform but no Velocity is one of the tiled
    // floor cells spawned in SpawnWorld -- the player already has both, so
    // this deliberately skips it and never gives it a Sprite: it's drawn as
    // a plain highlighted rect in Render() instead (see RenderHud), same as
    // audio_demo/camera_demo's state indicators.
    for ( auto [ entity, transform ] : m_Registry.View<Transform>() )
    {
        if ( m_Registry.HasComponent<Velocity>( entity ) ) continue;
        auto result = m_Registry.AddComponent<Sprite>( entity, Sprite{ m_TileTexture.get(), std::nullopt, kTilePath } );
        if ( !result ) result.LogError();
    }

    m_SpritesAttached = true;
}

void CameraFollowDemoState::UpdatePlayerVelocity()
{
    auto result = m_Registry.GetComponent<Velocity>( m_Player );
    if ( !result ) return;

    Velocity& velocity = result.Value().get();
    velocity.m_DX = (m_Right ? kPlayerSpeed : 0.0f) - (m_Left ? kPlayerSpeed : 0.0f);
    velocity.m_DY = (m_Down  ? kPlayerSpeed : 0.0f) - (m_Up   ? kPlayerSpeed : 0.0f);
}

void CameraFollowDemoState::AdjustZoom(float inDelta)
{
    auto result = m_Registry.GetComponent<Camera>( m_Player );
    if ( !result ) return;
    Camera& camera = result.Value().get();
    camera.m_Zoom = std::clamp( camera.m_Zoom + inDelta, kZoomMin, kZoomMax );
}

void CameraFollowDemoState::ToggleSmoothing()
{
    auto result = m_Registry.GetComponent<Camera>( m_Player );
    if ( !result ) return;
    Camera& camera = result.Value().get();
    camera.m_Smoothing = ( camera.m_Smoothing > 0.0f ) ? 0.0f : kSmoothingRate;
}

void CameraFollowDemoState::RenderHud(asge::video::IRenderer &inRenderer) const
{
    auto cameraResult = m_Registry.GetComponent<Camera>( m_Player );
    if ( !cameraResult ) return;
    Camera const& camera = cameraResult.Value().get();

    // Landmarks and the player marker are drawn straight via DrawRect/
    // DrawCircle rather than through RenderSystem -- they still land in the
    // right place because IRenderer applies whatever camera CameraSystem
    // just set to every draw call, ECS-driven or not (see IRenderer::
    // SetCamera's doc comment).
    for ( auto const& landmark : kLandmarks )
    {
        inRenderer.DrawCircle(
            asge::math::Int2{ static_cast<int>(landmark.m_Pos.x()), static_cast<int>(landmark.m_Pos.y()) },
            24, landmark.m_Color, true
        );
    }

    if ( auto transform = m_Registry.GetComponent<Transform>( m_Player ) )
    {
        Transform const& t = transform.Value().get();
        inRenderer.DrawRect(
            asge::math::Rect{
                t.m_WorldCoordinates.x() - kPlayerSize * 0.5f, t.m_WorldCoordinates.y() - kPlayerSize * 0.5f,
                kPlayerSize, kPlayerSize
            },
            RGBA_Color{ 250, 250, 255, 255 }, true
        );
    }

    // HUD is drawn in screen space, independent of the ECS camera -- reset
    // it first so the bars below land at fixed screen positions regardless
    // of where the player currently is in the world. CameraSystem reads
    // IRenderer::GetCamera() as its own starting point every frame (that's
    // what makes m_Smoothing's ease-toward-target math continuous across
    // frames), so this must be put back before returning -- leaving the
    // identity camera set here would make next frame's CameraSystem think
    // the view had snapped back to the origin.
    asge::video::Camera const worldCamera = inRenderer.GetCamera();
    inRenderer.SetCamera( asge::video::Camera{} );

    // Zoom bar (top-left): fill tracks Camera::m_Zoom between kZoomMin/Max.
    asge::math::Rect const zoomTrack{ 20.0f, 20.0f, 160.0f, 16.0f };
    float const zoomFrac = (camera.m_Zoom - kZoomMin) / (kZoomMax - kZoomMin);
    inRenderer.DrawRect(zoomTrack, RGBA_Color{ 90, 90, 100, 255 }, false);
    inRenderer.DrawRect(
        asge::math::Rect{ zoomTrack.m_X, zoomTrack.m_Y, zoomTrack.m_Width * zoomFrac, zoomTrack.m_Height },
        RGBA_Color{ 100, 170, 240, 255 }, true
    );

    // "SMOOTH" indicator (below the zoom bar): lit while Camera::m_Smoothing > 0.
    inRenderer.DrawRect(
        asge::math::Rect{ 20.0f, 46.0f, 30.0f, 16.0f },
        camera.m_Smoothing > 0.0f ? RGBA_Color{ 80, 220, 140, 255 } : RGBA_Color{ 55, 55, 60, 255 },
        true
    );

    inRenderer.SetCamera( worldCamera );
}

std::optional<asge::game::state::Transition<int>>
CameraFollowDemoState::Update(float inDeltaTime, [[maybe_unused]] asge::input::InputState const &inInput)
{
    UpdatePlayerVelocity();
    asge::game::systems::MovementSystem( m_Registry, inDeltaTime );
    asge::game::systems::TransformPropagationSystem( m_Registry );
    // CameraSystem itself runs inside RenderPipeline (see Render()) -- same
    // reasoning as animation_demo's m_LastDeltaTime capture.
    m_LastDeltaTime = inDeltaTime;
    return std::nullopt;
}

void CameraFollowDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 15, 18, 22, 255 });
    EnsureSpritesAttached(inRenderer);

    // The one call this whole demo exists to show off: CameraSystem (aims
    // IRenderer's camera at ActiveCamera's entity) then RenderSystem (draws
    // every Transform+Sprite whose destination rect is still in view).
    asge::game::systems::RenderPipeline( m_Registry, inRenderer, m_LastDeltaTime );

    RenderHud( inRenderer );
}

void CameraFollowDemoState::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>();
    if ( !keyEvent ) return;

    bool const pressed = keyEvent->s_Type == asge::event::EventType::KEYBOARD_KEY_PRESSED;
    switch ( keyEvent->s_Keycode )
    {
    case Keycode::W: m_Up = pressed; break;
    case Keycode::S: m_Down = pressed; break;
    case Keycode::A: m_Left = pressed; break;
    case Keycode::D: m_Right = pressed; break;
    default: break;
    }

    if ( !pressed || keyEvent->s_Repeat ) return; // rest are edge-triggered: one step per physical press

    if ( keyEvent->s_Keycode == Keycode::UP )        AdjustZoom( kZoomStep );
    else if ( keyEvent->s_Keycode == Keycode::DOWN ) AdjustZoom( -kZoomStep );
    else if ( keyEvent->s_Keycode == Keycode::F )    ToggleSmoothing();
}

CameraFollowDemoGame::CameraFollowDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    // ASGE_CAMERA_FOLLOW_DEMO_ASSET_DIR is injected by CMakeLists.txt;
    // mounted once so the tile texture is loaded by virtual path (see
    // EnsureSpritesAttached) instead of a hardcoded OS path.
    auto mountResult = m_Vfs.Mount("textures", ASGE_CAMERA_FOLLOW_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    SetInitialState(0);
}

std::unique_ptr<CameraFollowDemoGame::StateType> CameraFollowDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<CameraFollowDemoState>( m_SceneManager.GetRegistry(), m_Assets );
}

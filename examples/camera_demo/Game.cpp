#include "Game.hpp"

#include <algorithm>
#include <array>

namespace
{

using asge::input::Keycode;
using asge::media::RGBA_Color;
using asge::video::Camera;
using asge::video::Viewport;

constexpr float kWindowW = 800.0f;
constexpr float kWindowH = 600.0f;

constexpr float kTileSize  = 100.0f;
constexpr int   kGridCols  = 16;
constexpr int   kGridRows  = 12;
constexpr float kWorldW    = kGridCols * kTileSize;
constexpr float kWorldH    = kGridRows * kTileSize;

constexpr float kPanSpeed = 400.0f; // world units/second
constexpr float kZoomStep = 0.1f;   // camera zoom per scroll wheel unit
constexpr float kMinZoom  = 0.3f;
constexpr float kMaxZoom  = 3.0f;

// Backdrop is drawn in full-window screen space; the viewport below is the
// smaller area inside it the minimap's own camera actually renders into.
constexpr asge::math::Rect kMinimapBackdrop{ 620.0f, 10.0f, 170.0f, 130.0f };
constexpr asge::math::Rect kMinimapViewport{ 625.0f, 15.0f, 160.0f, 120.0f };

constexpr float kLandmarkRadius = 35.0f;

struct Landmark
{
    asge::math::Float2 m_Pos;
    RGBA_Color          m_Color;
};

std::array<Landmark, 5> const kLandmarks{ {
    { { 150.0f,  150.0f }, RGBA_Color{ 220,  60,  60, 255 } },
    { { 1450.0f, 150.0f }, RGBA_Color{  60, 200,  90, 255 } },
    { { 800.0f,  600.0f }, RGBA_Color{ 240, 200,  60, 255 } },
    { { 150.0f, 1050.0f }, RGBA_Color{  90, 170, 240, 255 } },
    { { 1450.0f,1050.0f }, RGBA_Color{ 200,  90, 220, 255 } },
} };

}

void CameraDemoState::UpdateCamera(float inDeltaTime, asge::input::InputState const &inInput)
{
    float const dx = (inInput.IsKeyDown(Keycode::D) ? kPanSpeed : 0.0f)
                    - (inInput.IsKeyDown(Keycode::A) ? kPanSpeed : 0.0f);
    float const dy = (inInput.IsKeyDown(Keycode::S) ? kPanSpeed : 0.0f)
                    - (inInput.IsKeyDown(Keycode::W) ? kPanSpeed : 0.0f);

    m_Camera.m_X += dx * inDeltaTime;
    m_Camera.m_Y += dy * inDeltaTime;
    m_Camera.m_Zoom = std::clamp(
        m_Camera.m_Zoom + inInput.GetScrollDelta().y() * kZoomStep, kMinZoom, kMaxZoom
    );

    // Keep the visible area inside the world when it fits within it,
    // otherwise center the (now larger than the world) view on it.
    float const visibleW = kWindowW / m_Camera.m_Zoom;
    float const visibleH = kWindowH / m_Camera.m_Zoom;
    m_Camera.m_X = visibleW < kWorldW
        ? std::clamp(m_Camera.m_X, 0.0f, kWorldW - visibleW)
        : (kWorldW - visibleW) * 0.5f;
    m_Camera.m_Y = visibleH < kWorldH
        ? std::clamp(m_Camera.m_Y, 0.0f, kWorldH - visibleH)
        : (kWorldH - visibleH) * 0.5f;
}

void CameraDemoState::RenderWorld(asge::video::IRenderer &inRenderer) const
{
    for ( int row = 0; row < kGridRows; ++row )
    {
        for ( int col = 0; col < kGridCols; ++col )
        {
            bool const light = (row + col) % 2 == 0;
            inRenderer.DrawRect(
                asge::math::Rect{
                    static_cast<float>(col) * kTileSize, static_cast<float>(row) * kTileSize,
                    kTileSize, kTileSize
                },
                light ? RGBA_Color{ 60, 65, 80, 255 } : RGBA_Color{ 40, 44, 56, 255 },
                true
            );
        }
    }

    // World border, so panning past the edge of the map is obvious.
    inRenderer.DrawRect(asge::math::Rect{ 0.0f, 0.0f, kWorldW, kWorldH }, RGBA_Color{ 240, 240, 245, 255 }, false);

    for ( auto const& landmark : kLandmarks )
    {
        inRenderer.DrawCircle(
            asge::math::Int2{ static_cast<int>(landmark.m_Pos.x()), static_cast<int>(landmark.m_Pos.y()) },
            static_cast<int>(kLandmarkRadius), landmark.m_Color, true
        );
    }
}

void CameraDemoState::RenderMinimap(asge::video::IRenderer &inRenderer) const
{
    inRenderer.SetViewport(kMinimapViewport);

    // Zoom chosen so the whole world fits inside the minimap viewport.
    float const zoom = std::min(kMinimapViewport.m_Width / kWorldW, kMinimapViewport.m_Height / kWorldH);
    inRenderer.SetCamera(Camera{ .m_X = 0.0f, .m_Y = 0.0f, .m_Zoom = zoom });

    RenderWorld(inRenderer);

    // The main camera's visible area, in world space -- SetCamera above
    // means this lands in minimap space exactly like everything RenderWorld
    // just drew, with no extra math needed here.
    asge::math::Rect const visible{
        m_Camera.m_X, m_Camera.m_Y, kWindowW / m_Camera.m_Zoom, kWindowH / m_Camera.m_Zoom
    };
    inRenderer.DrawRect(visible, RGBA_Color{ 255, 255, 255, 255 }, false);
}

std::optional<asge::game::state::Transition<int>>
CameraDemoState::Update(float inDeltaTime, asge::input::InputState const &inInput)
{
    UpdateCamera(inDeltaTime, inInput);
    return std::nullopt;
}

void CameraDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear(RGBA_Color{ 15, 15, 20, 255 });

    inRenderer.SetViewport(Viewport{ 0.0f, 0.0f, kWindowW, kWindowH });
    inRenderer.SetCamera(m_Camera);
    RenderWorld(inRenderer);

    // Minimap backdrop, drawn in full-window screen space (identity camera)
    // before RenderMinimap below switches to its own viewport/camera.
    inRenderer.SetCamera(Camera{});
    inRenderer.DrawRect(kMinimapBackdrop, RGBA_Color{ 10, 10, 14, 230 }, true);
    inRenderer.DrawRect(kMinimapBackdrop, RGBA_Color{ 200, 200, 210, 255 }, false);

    RenderMinimap(inRenderer);

    // Restore full-window/main-camera state -- RenderMinimap leaves both
    // pointed at its own viewport/camera, and Render() should hand the
    // renderer back the way it found it.
    inRenderer.SetViewport(Viewport{ 0.0f, 0.0f, kWindowW, kWindowH });
    inRenderer.SetCamera(m_Camera);
}

void CameraDemoState::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>();
    if ( !keyEvent ) return;
    if ( keyEvent->s_Type != asge::event::EventType::KEYBOARD_KEY_PRESSED ) return;
    if ( keyEvent->s_Keycode == Keycode::R ) m_Camera = Camera{};
}

CameraDemoGame::CameraDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    SetInitialState(0);
}

std::unique_ptr<CameraDemoGame::StateType> CameraDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<CameraDemoState>();
}

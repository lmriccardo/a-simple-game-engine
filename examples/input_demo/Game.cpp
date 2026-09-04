#include "Game.hpp"

#include <algorithm>

using asge::input::Keycode;
using asge::input::MouseButton;

namespace
{
constexpr float kBoxSpeed      = 250.0f; // pixels/second
constexpr float kMinBoxSize    = 20.0f;
constexpr float kMaxBoxSize    = 120.0f;
constexpr float kScrollToSize  = 6.0f;   // pixels of box size per scroll unit
constexpr int   kCursorRadius  = 12;
constexpr int   kMarkRadius    = 5;
constexpr float kWindowW       = 800.0f;
constexpr float kWindowH       = 600.0f;
}

void InputDemoGame::UpdatePlayerBox(float inDeltaTime, asge::input::InputState const &inInput)
{
    // Continuous, held-down movement -- exactly what IsKeyDown polling replaces
    // the hand-rolled m_Up/m_Down/m_Left/m_Right + OnSystemEvent pattern for.
    float const dx = (inInput.IsKeyDown(Keycode::D) ? kBoxSpeed : 0.0f)
                    - (inInput.IsKeyDown(Keycode::A) ? kBoxSpeed : 0.0f);
    float const dy = (inInput.IsKeyDown(Keycode::S) ? kBoxSpeed : 0.0f)
                    - (inInput.IsKeyDown(Keycode::W) ? kBoxSpeed : 0.0f);

    m_BoxPos += asge::math::Float2{ dx, dy } * inDeltaTime;
    m_BoxPos = {
        std::clamp( m_BoxPos.x(), 0.0f, kWindowW - m_BoxSize ),
        std::clamp( m_BoxPos.y(), 0.0f, kWindowH - m_BoxSize )
    };
}

void InputDemoGame::UpdateBackgroundToggle(asge::input::InputState const &inInput)
{
    // Edge-triggered: only flips once per press, unlike IsKeyDown above.
    if ( inInput.IsKeyPressed(Keycode::SPACE) )
        m_LightBackground = !m_LightBackground;
}

void InputDemoGame::UpdateBoxSizeFromScroll(asge::input::InputState const &inInput)
{
    m_BoxSize += inInput.GetScrollDelta().y() * kScrollToSize;
    m_BoxSize = std::clamp( m_BoxSize, kMinBoxSize, kMaxBoxSize );
}

void InputDemoGame::UpdateCursor(asge::input::InputState const &inInput)
{
    m_CursorPos = inInput.GetMousePosition();
    m_CursorHeld = inInput.IsMouseButtonDown(MouseButton::LEFT);
}

void InputDemoGame::UpdateMarks(asge::input::InputState const &inInput)
{
    // Right-click drops a mark at the cursor -- edge-triggered, same as the
    // SPACE toggle above but for a mouse button instead of a key.
    if ( !inInput.IsMouseButtonPressed(MouseButton::RIGHT) ) return;

    m_Marks[m_NextMarkSlot] = inInput.GetMousePosition();
    m_NextMarkSlot = (m_NextMarkSlot + 1) % kMaxMarks;
    m_MarkCount = std::min( m_MarkCount + 1, kMaxMarks );
}

void InputDemoGame::Update(float inDeltaTime, asge::input::InputState const &inInput)
{
    UpdatePlayerBox( inDeltaTime, inInput );
    UpdateBackgroundToggle( inInput );
    UpdateBoxSizeFromScroll( inInput );
    UpdateCursor( inInput );
    UpdateMarks( inInput );
}

void InputDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear( m_LightBackground
        ? asge::media::RGBA_Color{ 225, 225, 230, 255 }
        : asge::media::RGBA_Color{ 20, 20, 25, 255 } );

    // Marks dropped by right-click.
    for ( std::size_t i = 0; i < m_MarkCount; ++i )
    {
        inRenderer.DrawCircle(
            asge::math::Int2{ static_cast<int>(m_Marks[i].x()), static_cast<int>(m_Marks[i].y()) },
            kMarkRadius, { 240, 200, 60, 255 }, true );
    }

    // WASD-controlled box.
    inRenderer.DrawRect(
        asge::math::Rect{ m_BoxPos.x(), m_BoxPos.y(), m_BoxSize, m_BoxSize },
        { 220, 60, 60, 255 }, true );

    // Mouse cursor: filled while left-click is held, outline otherwise. Both the
    // position and the held-state come from InputState polling, cached in Update().
    inRenderer.DrawCircle(
        asge::math::Int2{ static_cast<int>(m_CursorPos.x()), static_cast<int>(m_CursorPos.y()) },
        kCursorRadius, { 90, 170, 240, 255 }, m_CursorHeld );
}

void InputDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Deliberately empty -- every reaction to input in this example comes from
    // polling InputState in Update() instead.
}

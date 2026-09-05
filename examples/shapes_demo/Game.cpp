#include "Game.hpp"

#include <cmath>

namespace
{
constexpr float LINE_SWEEP_SPEED = 1.5f; // radians per second
constexpr float LINE_RADIUS      = 100.0f;
const asge::math::Float2 LINE_ORIGIN{ 400.0f, 300.0f };
}

std::optional<asge::game::state::Transition<int>>
ShapesDemoState::Update(float inDeltaTime, [[maybe_unused]] asge::input::InputState const& inInput)
{
    m_LineAngle += LINE_SWEEP_SPEED * inDeltaTime;
    return std::nullopt;
}

void ShapesDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 20, 20, 20, 255 });

    // Filled rect
    inRenderer.DrawRect(
        asge::math::Rect{ 80.0f, 80.0f, 160.0f, 120.0f },
        { 220, 60, 60, 255 },
        true
    );

    // Outlined rect
    inRenderer.DrawRect(
        asge::math::Rect{ 560.0f, 80.0f, 160.0f, 120.0f },
        { 60, 200, 90, 255 },
        false
    );

    // Sweeping line, to exercise DrawLine
    asge::math::Float2 tip{
        LINE_ORIGIN.x() + std::cos(m_LineAngle) * LINE_RADIUS,
        LINE_ORIGIN.y() + std::sin(m_LineAngle) * LINE_RADIUS
    };
    inRenderer.DrawLine(LINE_ORIGIN, tip, { 80, 160, 240, 255 });

    // Filled circle
    inRenderer.DrawCircle(
        asge::math::Int2{ 150, 480 }, 70,
        { 240, 200, 60, 255 },
        true
    );

    // Outlined circle
    inRenderer.DrawCircle(
        asge::math::Int2{ 650, 480 }, 70,
        { 200, 90, 220, 255 },
        false
    );
}

void ShapesDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
}

ShapesDemoGame::ShapesDemoGame(asge::video::IRenderer& inRenderer)
: Game(inRenderer)
{
    SetInitialState(0);
}

std::unique_ptr<ShapesDemoGame::StateType> ShapesDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<ShapesDemoState>();
}

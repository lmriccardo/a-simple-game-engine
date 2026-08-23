#include <ASGE/Input/InputState.hpp>
#include <ASGE/Events/Events.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::event::KeyboardEvent;
using asge::event::MouseButtonEvent;
using asge::event::MouseMotionEvent;
using asge::event::MouseWheelEvent;
using asge::event::QuitEvent;
using asge::event::SystemEvent;
using asge::input::InputState;
using asge::input::Keycode;
using asge::input::MouseButton;

SystemEvent KeyEvent(Keycode inKey, bool inDown)
{
    KeyboardEvent e{};
    e.s_Keycode = inKey;
    e.s_Down = inDown;
    return SystemEvent{ std::move(e) };
}

SystemEvent MouseButtonEv(MouseButton inButton, bool inDown)
{
    MouseButtonEvent e{};
    e.s_Button = inButton;
    e.s_Down = inDown;
    return SystemEvent{ std::move(e) };
}

SystemEvent MotionEvent(float inX, float inY)
{
    MouseMotionEvent e{};
    e.s_Position = { inX, inY };
    return SystemEvent{ std::move(e) };
}

SystemEvent WheelEvent(float inX, float inY)
{
    MouseWheelEvent e{};
    e.s_Scroll = { inX, inY };
    return SystemEvent{ std::move(e) };
}

// ─── Keyboard ───────────────────────────────────────────────────────────────

TEST(InputStateTest, IsKeyDown_FalseBeforeAnyEvent)
{
    InputState state;
    EXPECT_FALSE( state.IsKeyDown(Keycode::W) );
}

TEST(InputStateTest, IsKeyDown_ReflectsLatestKeyEvent)
{
    InputState state;
    state.Consume( KeyEvent(Keycode::W, true) );
    EXPECT_TRUE( state.IsKeyDown(Keycode::W) );

    state.Consume( KeyEvent(Keycode::W, false) );
    EXPECT_FALSE( state.IsKeyDown(Keycode::W) );
}

TEST(InputStateTest, IsKeyPressed_TrueOnlyOnTheFrameItWentDown)
{
    InputState state;
    state.NewFrame();
    state.Consume( KeyEvent(Keycode::W, true) );
    EXPECT_TRUE( state.IsKeyPressed(Keycode::W) );

    // Held into the next frame - no longer a fresh press.
    state.NewFrame();
    EXPECT_TRUE( state.IsKeyDown(Keycode::W) );
    EXPECT_FALSE( state.IsKeyPressed(Keycode::W) );
}

TEST(InputStateTest, IsKeyReleased_TrueOnlyOnTheFrameItWentUp)
{
    InputState state;
    state.NewFrame();
    state.Consume( KeyEvent(Keycode::W, true) );
    state.NewFrame();

    state.Consume( KeyEvent(Keycode::W, false) );
    EXPECT_TRUE( state.IsKeyReleased(Keycode::W) );

    state.NewFrame();
    EXPECT_FALSE( state.IsKeyDown(Keycode::W) );
    EXPECT_FALSE( state.IsKeyReleased(Keycode::W) );
}

// ─── Mouse buttons ──────────────────────────────────────────────────────────

TEST(InputStateTest, MouseButton_DownPressedReleasedFollowTheSameFrameRule)
{
    InputState state;
    state.NewFrame();
    state.Consume( MouseButtonEv(MouseButton::LEFT, true) );
    EXPECT_TRUE( state.IsMouseButtonDown(MouseButton::LEFT) );
    EXPECT_TRUE( state.IsMouseButtonPressed(MouseButton::LEFT) );

    state.NewFrame();
    EXPECT_TRUE( state.IsMouseButtonDown(MouseButton::LEFT) );
    EXPECT_FALSE( state.IsMouseButtonPressed(MouseButton::LEFT) );

    state.Consume( MouseButtonEv(MouseButton::LEFT, false) );
    EXPECT_TRUE( state.IsMouseButtonReleased(MouseButton::LEFT) );
    EXPECT_FALSE( state.IsMouseButtonDown(MouseButton::LEFT) );
}

// ─── Mouse position / delta ─────────────────────────────────────────────────

TEST(InputStateTest, MousePosition_ReflectsLatestMotionEvent)
{
    InputState state;
    state.Consume( MotionEvent(10.f, 20.f) );

    EXPECT_FLOAT_EQ( state.GetMousePosition().x(), 10.f );
    EXPECT_FLOAT_EQ( state.GetMousePosition().y(), 20.f );
}

TEST(InputStateTest, MouseDelta_IsDifferenceSinceLastNewFrame)
{
    InputState state;
    state.NewFrame();
    state.Consume( MotionEvent(10.f, 20.f) );

    EXPECT_FLOAT_EQ( state.GetMouseDelta().x(), 10.f );
    EXPECT_FLOAT_EQ( state.GetMouseDelta().y(), 20.f );

    state.NewFrame();
    state.Consume( MotionEvent(15.f, 20.f) );

    EXPECT_FLOAT_EQ( state.GetMouseDelta().x(), 5.f );
    EXPECT_FLOAT_EQ( state.GetMouseDelta().y(), 0.f );
}

// ─── Scroll ─────────────────────────────────────────────────────────────────

TEST(InputStateTest, ScrollDelta_AccumulatesWithinAFrameAndResetsOnNewFrame)
{
    InputState state;
    state.NewFrame();
    state.Consume( WheelEvent(0.f, 3.f) );
    state.Consume( WheelEvent(0.f, 2.f) );

    EXPECT_FLOAT_EQ( state.GetScrollDelta().y(), 5.f );

    state.NewFrame();
    EXPECT_FLOAT_EQ( state.GetScrollDelta().y(), 0.f );
}

// ─── Unrelated events ───────────────────────────────────────────────────────

TEST(InputStateTest, Consume_IgnoresUnrelatedEventsWithoutCrashing)
{
    InputState state;
    EXPECT_NO_THROW( state.Consume( SystemEvent{ QuitEvent{} } ) );
    EXPECT_FALSE( state.IsKeyDown(Keycode::W) );
}

}

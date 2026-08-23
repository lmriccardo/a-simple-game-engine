#include <ASGE/Events/Events.hpp>

#include <SDL3/SDL_events.h>

#include <gtest/gtest.h>

namespace
{

using asge::event::EventType;
using asge::event::MouseButtonEvent;
using asge::event::MouseMotionEvent;
using asge::event::MouseWheelEvent;
using asge::event::SystemEvent;
using asge::event::_internal::ProcessEvent;
using asge::input::MouseButton;

SDL_Event MakeSdlEvent()
{
    // Zero-init so untouched fields (padding, reserved, ...) don't carry
    // garbage into the translation.
    SDL_Event event{};
    return event;
}

// ─── Mouse motion ───────────────────────────────────────────────────────────

TEST(MouseEventTranslationTest, Motion_ProducesMouseMotionEvent)
{
    SDL_Event sdlEvent = MakeSdlEvent();
    sdlEvent.type = SDL_EVENT_MOUSE_MOTION;
    sdlEvent.motion.windowID = 7u;
    sdlEvent.motion.which = 1u;
    sdlEvent.motion.x = 12.f;
    sdlEvent.motion.y = 34.f;
    sdlEvent.motion.xrel = 2.f;
    sdlEvent.motion.yrel = -1.f;

    SystemEvent sysEvent = ProcessEvent( &sdlEvent );

    auto const* motion = sysEvent.TryGet<MouseMotionEvent>();
    ASSERT_NE( motion, nullptr );
    EXPECT_EQ( motion->s_Type, EventType::MOUSE_MOTION );
    EXPECT_EQ( motion->s_WindowId, 7u );
    EXPECT_EQ( motion->s_MouseId, 1u );
    EXPECT_FLOAT_EQ( motion->s_Position.x(), 12.f );
    EXPECT_FLOAT_EQ( motion->s_Position.y(), 34.f );
    EXPECT_FLOAT_EQ( motion->s_Delta.x(), 2.f );
    EXPECT_FLOAT_EQ( motion->s_Delta.y(), -1.f );
}

// ─── Mouse buttons ──────────────────────────────────────────────────────────

TEST(MouseEventTranslationTest, ButtonDown_ProducesPressedEvent)
{
    SDL_Event sdlEvent = MakeSdlEvent();
    sdlEvent.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    sdlEvent.button.windowID = 3u;
    sdlEvent.button.which = 1u;
    sdlEvent.button.button = SDL_BUTTON_LEFT;
    sdlEvent.button.down = true;
    sdlEvent.button.clicks = 1u;
    sdlEvent.button.x = 5.f;
    sdlEvent.button.y = 6.f;

    SystemEvent sysEvent = ProcessEvent( &sdlEvent );

    auto const* button = sysEvent.TryGet<MouseButtonEvent>();
    ASSERT_NE( button, nullptr );
    EXPECT_EQ( button->s_Type, EventType::MOUSE_BUTTON_PRESSED );
    EXPECT_EQ( button->s_Button, MouseButton::LEFT );
    EXPECT_TRUE( button->s_Down );
    EXPECT_EQ( button->s_Clicks, 1u );
    EXPECT_FLOAT_EQ( button->s_Position.x(), 5.f );
    EXPECT_FLOAT_EQ( button->s_Position.y(), 6.f );
}

TEST(MouseEventTranslationTest, ButtonUp_ProducesReleasedEvent)
{
    SDL_Event sdlEvent = MakeSdlEvent();
    sdlEvent.type = SDL_EVENT_MOUSE_BUTTON_UP;
    sdlEvent.button.button = SDL_BUTTON_RIGHT;
    sdlEvent.button.down = false;

    SystemEvent sysEvent = ProcessEvent( &sdlEvent );

    auto const* button = sysEvent.TryGet<MouseButtonEvent>();
    ASSERT_NE( button, nullptr );
    EXPECT_EQ( button->s_Type, EventType::MOUSE_BUTTON_RELEASED );
    EXPECT_EQ( button->s_Button, MouseButton::RIGHT );
    EXPECT_FALSE( button->s_Down );
}

// ─── Mouse wheel ────────────────────────────────────────────────────────────

TEST(MouseEventTranslationTest, Wheel_ProducesWheelEvent)
{
    SDL_Event sdlEvent = MakeSdlEvent();
    sdlEvent.type = SDL_EVENT_MOUSE_WHEEL;
    sdlEvent.wheel.windowID = 9u;
    sdlEvent.wheel.which = 1u;
    sdlEvent.wheel.x = 0.f;
    sdlEvent.wheel.y = 3.f;
    sdlEvent.wheel.mouse_x = 40.f;
    sdlEvent.wheel.mouse_y = 50.f;

    SystemEvent sysEvent = ProcessEvent( &sdlEvent );

    auto const* wheel = sysEvent.TryGet<MouseWheelEvent>();
    ASSERT_NE( wheel, nullptr );
    EXPECT_EQ( wheel->s_Type, EventType::MOUSE_WHEEL_MOTION );
    EXPECT_EQ( wheel->s_WindowId, 9u );
    EXPECT_FLOAT_EQ( wheel->s_Scroll.y(), 3.f );
    EXPECT_FLOAT_EQ( wheel->s_Position.x(), 40.f );
    EXPECT_FLOAT_EQ( wheel->s_Position.y(), 50.f );
}

// ─── Unrelated events must not be misclassified as mouse events ────────────

TEST(MouseEventTranslationTest, QuitEvent_IsNotAMouseEvent)
{
    SDL_Event sdlEvent = MakeSdlEvent();
    sdlEvent.type = SDL_EVENT_QUIT;

    SystemEvent sysEvent = ProcessEvent( &sdlEvent );

    EXPECT_EQ( sysEvent.TryGet<MouseMotionEvent>(), nullptr );
    EXPECT_EQ( sysEvent.TryGet<MouseButtonEvent>(), nullptr );
    EXPECT_EQ( sysEvent.TryGet<MouseWheelEvent>(), nullptr );
}

}

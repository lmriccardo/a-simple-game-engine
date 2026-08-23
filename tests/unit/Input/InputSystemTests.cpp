#include <ASGE/Input/InputSystem.hpp>
#include <ASGE/Events/Events.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::event::KeyboardEvent;
using asge::event::SystemEvent;
using asge::input::InputSystem;
using asge::input::Keycode;

SystemEvent KeyEvent(Keycode inKey, bool inDown)
{
    KeyboardEvent e{};
    e.s_Keycode = inKey;
    e.s_Down = inDown;
    return SystemEvent{ std::move(e) };
}

TEST(InputSystemTest, ProcessEvent_UpdatesGetState)
{
    InputSystem system;
    EXPECT_FALSE( system.GetState().IsKeyDown(Keycode::W) );

    system.ProcessEvent( KeyEvent(Keycode::W, true) );
    EXPECT_TRUE( system.GetState().IsKeyDown(Keycode::W) );
}

TEST(InputSystemTest, NewFrame_DrivesEdgeDetectionOnTheUnderlyingState)
{
    InputSystem system;

    system.NewFrame();
    system.ProcessEvent( KeyEvent(Keycode::W, true) );
    EXPECT_TRUE( system.GetState().IsKeyPressed(Keycode::W) );

    // Held into the next frame - no longer a fresh press, same rule InputState follows.
    system.NewFrame();
    EXPECT_TRUE( system.GetState().IsKeyDown(Keycode::W) );
    EXPECT_FALSE( system.GetState().IsKeyPressed(Keycode::W) );
}

}

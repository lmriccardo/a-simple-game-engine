#pragma once

#include <cstdint>
#include <concepts>
#include <type_traits>

namespace asge::event
{

#define EVENT_UKNOWN 0

/* Events releated to the Window */
enum class WindowEventType : std::uint32_t
{
    NONE           = EVENT_UKNOWN,
    QUIT           = 0x00000100u,  // User-requested quit
    WINDOW_RESIZED = 0x00000206u,  // Window has been resized
};

/* Events releated to the user-input */
enum class InputEventType : std::uint32_t
{
    NONE                    = EVENT_UKNOWN, // None (do not remove)
    KEY_PRESSED             = 0x00000300u,  // A key is pressed
    KEY_RELEASED,                           // A key is released
    MOUSE_MOTION,                           // Mouse moved
    MOUSE_BUTTON_PRESSED,                   // Mouse button is pressed
    MOUSE_BUTTON_RELEASED,                  // Mouse button is released
    MOUSE_WHEEL_MOTION                      // Mouse wheel motion
};

enum class SystemEventType
{
    UKNOWN,
    QUIT,
    WINDOW,
    KEYBOARD
};

}
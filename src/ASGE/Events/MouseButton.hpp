#pragma once

#include <cstdint>

namespace asge::input
{

/* Virtual mouse button codes. Values match SDL's SDL_BUTTON_* indices
 * directly so translation is a plain static_cast, same approach as Keycode. */
enum class MouseButton : std::uint8_t
{
    UNKNOWN,
    LEFT    = 1u,
    MIDDLE  = 2u,
    RIGHT   = 3u,
    X1      = 4u,
    X2      = 5u
};

}

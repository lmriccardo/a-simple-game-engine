#pragma once

#include <cstdint>

namespace asge::input
{

/**
 * @brief Virtual keymaps from keyboard input.
 */
enum class Keycode : std::uint32_t
{
    UNKNOWN,
    ESCAPE,
    SPACE,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    NUM_0,
    NUM_1,
    NUM_2,
    NUM_3,
    NUM_4,
    NUM_5,
    NUM_6,
    NUM_7,
    NUM_8,
    NUM_9,

    UP,
    DOWN,
    LEFT,
    RIGHT,

    COUNT // Sentinel - the number of known keycodes, not a real key
};

enum class Keymod : std::uint16_t
{
    NONE,
    LSHIFT  = 0x0001u, // 1
    RSHIFT  = 0x0002u, // 2
    LCTRL   = 0x0040u, // 64
    RCTRL   = 0x0080u, // 128
    LALT    = 0x0100u, // 256
    RALT    = 0x0200u, // 512
    CAPS    = 0x2000u, // 8192
    MODE    = 0x4000u, // 16384 (!AltGr)
};

}
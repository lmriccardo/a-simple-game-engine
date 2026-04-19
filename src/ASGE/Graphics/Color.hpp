#pragma once

#include <cstdint>

namespace asge::graphics
{

struct RGBA_Color
{
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};
};

}
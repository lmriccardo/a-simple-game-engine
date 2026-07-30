#pragma once

#include <cstdint>

namespace asge::video
{

enum class GraphicsBackend : std::uint8_t
{
    SDL = 1,
    // OpenGL, Vulkan, DirectX -- future backends
};

}

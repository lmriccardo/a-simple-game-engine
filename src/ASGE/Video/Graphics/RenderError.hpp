#pragma once

#include <ASGE/Core/Errors.hpp>

namespace asge::errors
{

// ---------------------------------------------------------------------------------------------
// RENDER SYSTEM ERRORS
// ---------------------------------------------------------------------------------------------

enum class RenderError : std::uint8_t
{
    RenderRectFailed = 1,
    RenderLineFailed,
    RenderCircleFailed
};

inline str::String ToErrorString(RenderError e) noexcept
{
    switch (e)
    {
    case RenderError::RenderRectFailed: return "failed to render the rect";
    case RenderError::RenderLineFailed: return "failed to render the line";
    case RenderError::RenderCircleFailed: return "failed to render the circle";
    }
    return "uknown render error";
}

}

REGISTER_ASGE_ERROR(asge::errors::RenderError, "asge.render")

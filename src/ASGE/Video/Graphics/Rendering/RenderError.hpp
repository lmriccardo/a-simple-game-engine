#pragma once

#include <ASGE/Core/Errors.hpp>

namespace asge::errors
{

// ---------------------------------------------------------------------------------------------
// RENDER SYSTEM ERRORS
// ---------------------------------------------------------------------------------------------

enum class RenderError : std::uint8_t
{
    CreateRendererFailed = 1,
    TextureCreationFailed,
    TextureUpdateFailed,
    TextureSetColorModFailed,
    TextureGetColorModFailed,
    SetDrawColorFailed,
    RenderClearFailed,
    RenderPresentFailed,
    RenderRectFailed,
    RenderLineFailed,
    RenderCircleFailed,
    RenderTextureFailed,
};

inline str::String ToErrorString(RenderError e) noexcept
{
    switch (e)
    {
    case RenderError::CreateRendererFailed: return "failed to create the renderer";
    case RenderError::SetDrawColorFailed: return "failed to set the render draw color";
    case RenderError::RenderClearFailed: return "failed to clear the renderer";
    case RenderError::RenderPresentFailed: return "failed to present the renderer";
    case RenderError::RenderRectFailed: return "failed to render the rect";
    case RenderError::RenderLineFailed: return "failed to render the line";
    case RenderError::RenderCircleFailed: return "failed to render the circle";
    case RenderError::TextureCreationFailed: return "failed to create the texture";
    case RenderError::TextureUpdateFailed: return "failed to update the texture";
    case RenderError::RenderTextureFailed: return "failed to render a texture";
    case RenderError::TextureSetColorModFailed: return "unable to set texture color modulation";
    case RenderError::TextureGetColorModFailed: return "unable to get texture color modulation";
    }
    return "uknown render error";
}

}

REGISTER_ASGE_ERROR(asge::errors::RenderError, "asge.render")

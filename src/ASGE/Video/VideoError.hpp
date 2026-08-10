#pragma once

#include <ASGE/Core/Errors.hpp>

namespace asge::errors
{

// ---------------------------------------------------------------------------------------------
// VIDEO SYSTEM ERRORS
// ---------------------------------------------------------------------------------------------

enum class VideoError : std::uint8_t
{
    SubsystemInitFailed = 1,
    WindowCreationFailed,
    RendererCreationFailed
};

inline str::String ToErrorString(VideoError e) noexcept
{
    switch (e)
    {
    case VideoError::SubsystemInitFailed: return "failed to initialize the video subsystem";
    case VideoError::WindowCreationFailed: return "failed to create the window";
    case VideoError::RendererCreationFailed: return "failed to create the renderer";
    }
    return "uknown video error";
}

}

REGISTER_ASGE_ERROR(asge::errors::VideoError, "asge.video")

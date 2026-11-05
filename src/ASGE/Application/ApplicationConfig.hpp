#pragma once

#include <string>
#include <cstdint>

namespace asge
{

struct ApplicationConfig
{
    int           s_Width    {800};    // Window width in pixels
    int           s_Height   {600};    // Window height in pixels
    std::string   s_Title    {"ASGE"}; // Window title
    std::uint64_t s_TargetFps{120};    // Frame rate cap passed to time::TargetFPS
};

}
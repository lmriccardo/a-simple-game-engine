#pragma once

#include <string>
#include <cstdint>

namespace asge
{

struct ApplicationConfig
{
    int           s_Width    {800};
    int           s_Height   {600};
    std::string   s_Title    {"ASGE"};
    std::uint64_t s_TargetFps{120};
};

}
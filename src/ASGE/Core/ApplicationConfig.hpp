#pragma once

#include <string>

namespace asge
{

struct ApplicationConfig
{
    int         s_Width  {800};
    int         s_Height {600};
    std::string s_Title  {"ASGE"};
};

}
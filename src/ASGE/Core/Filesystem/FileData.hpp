#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <optional>

namespace asge::filesystem
{

using Path = std::filesystem::path;
using FileSize = uintmax_t;
using FileTime = std::filesystem::file_time_type;
using FileType = std::filesystem::file_type;

}
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

template<typename T>
struct FileResult
{
    using ret_type = T;

    std::optional<T> s_Result;
    std::error_code  s_Error;

    explicit operator bool() const 
    { return !s_Error; }

    T operator*() const
    {
        return *s_Result;
    }
};

}
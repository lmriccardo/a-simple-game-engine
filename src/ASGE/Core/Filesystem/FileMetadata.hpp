#pragma once

#include <filesystem>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Errors.hpp>
#include "FileData.hpp"

namespace asge::filesystem::meta
{

/* Checks if the input path exists or not */
bool Exists( Path const& inPath ) noexcept;

/* Checks if the input path is a regular file or not */
bool IsFile( Path const& inPath ) noexcept;

/* Checks whether the given path points to a directory. */
bool IsDirectory(Path const& inPath) noexcept;

/* Checks whether the given path points to a symbolic link. */
bool IsSymlink(Path const& inPath) noexcept;

/* Checks whether the given path points to a socket file. */
bool IsSocket(Path const& inPath) noexcept;

/* Checks whether the given path points to an empty file or directory. */
bool IsEmpty(Path const& inPath) noexcept;

/* Returns the size of the file in bytes. */
Result<FileSize> GetFileSize(Path const& inPath) noexcept;

/* Returns the last write time of the file or directory. */
Result<FileTime> GetLastModified(Path const& inPath) noexcept;

/* Returns the extension (if any) of the input regular file */
Result<str::String> GetExtension( Path const& inPath ) noexcept;

/* Returns the filename component of the path, including extension */
str::String GetFilename(Path const& inPath) noexcept;

/* Returns the stem of the filename, without extension */
str::String GetStem(Path const& inPath) noexcept;

}
#pragma once

#include <filesystem>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Errors.hpp>
#include "FileData.hpp"

namespace asge::filesystem::meta
{

/**
 * @brief Checks if the input path exists or not.
 *
 * False on any filesystem error too, e.g. an inaccessible parent directory.
 */
bool Exists( Path const& inPath ) noexcept;

/**
 * @brief Checks if the input path is a regular file or not.
 *
 * Directories, symlinks to directories, sockets and missing paths are not.
 */
bool IsFile( Path const& inPath ) noexcept;

/**
 * @brief Checks whether the given path points to a directory.
 *
 * Symlinks are followed, so a link to a directory counts.
 */
bool IsDirectory(Path const& inPath) noexcept;

/**
 * @brief Checks whether the given path points to a symbolic link.
 *
 * Looks at the link itself, not its target.
 */
bool IsSymlink(Path const& inPath) noexcept;

/**
 * @brief Checks whether the given path points to a socket file.
 *
 * Always false on platforms without such files.
 */
bool IsSocket(Path const& inPath) noexcept;

/**
 * @brief Checks whether the given path points to an empty file or directory.
 *
 * A file is empty at size 0, a directory when it has no entries. False on error.
 */
bool IsEmpty(Path const& inPath) noexcept;

/**
 * @brief Returns the size of the file in bytes.
 *
 * An error Result carrying the path in its detail if the size cannot be read
 * (missing path, or not a regular file).
 */
Result<FileSize> GetFileSize(Path const& inPath) noexcept;

/**
 * @brief Returns the last write time of the file or directory.
 *
 * The value is a std::filesystem file_time_type, which is not directly
 * comparable to system_clock time points.
 */
Result<FileTime> GetLastModified(Path const& inPath) noexcept;

/**
 * @brief Returns the extension (if any) of the input regular file.
 *
 * Includes the leading dot (".png"). An error Result if the path has no
 * extension.
 */
Result<str::String> GetExtension( Path const& inPath ) noexcept;

/**
 * @brief Returns the filename component of the path, including extension.
 *
 * Empty for a path that ends in a separator.
 */
str::String GetFilename(Path const& inPath) noexcept;

/**
 * @brief Returns the stem of the filename, without extension.
 *
 * Only the last extension is removed ("a.tar.gz" gives "a.tar").
 */
str::String GetStem(Path const& inPath) noexcept;

}
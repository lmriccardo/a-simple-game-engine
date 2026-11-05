#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <cstddef>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Strings.hpp>
#include "FileMetadata.hpp"
#include "FileData.hpp"

namespace asge::filesystem
{

/**
 * @brief Reads the input file content into a string.
 *
 * Returns an error Result if the file cannot be opened or read. The bytes are
 * returned as is, so a binary file works too, but ReadBinary is clearer.
 */
Result<str::String> ReadText( Path const& inPath );

/**
 * @brief Writes (overwriting) the input content into the given file.
 *
 * Creates the file if missing and truncates it otherwise. The parent
 * directory must already exist. Returns an error Result on failure.
 */
BoolResult WriteText( Path const& inPath, str::String const& inContent );

/**
 * @brief Copies (overwriting an existing destination) a file.
 *
 * Named Copy (not CopyFile) because Windows headers `#define` CopyFile to
 * CopyFileA/W. Returns an error Result if the source cannot be read or the
 * destination cannot be written.
 */
BoolResult Copy( Path const& inSource, Path const& inDestination ) noexcept;

/**
 * @brief Reads the entire contents of a file into memory as raw bytes.
 *
 * Opens the file in binary mode (no text-mode newline translation), so it
 * is safe for arbitrary binary data such as images, audio, or compressed
 * assets. For text files, prefer ReadText instead. Returns an error Result
 * if the file cannot be opened or read.
 */
Result<std::vector<std::byte>> ReadBinary( Path const& inPath );

}
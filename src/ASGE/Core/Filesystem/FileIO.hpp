#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Strings.hpp>
#include "FileMetadata.hpp"
#include "FileData.hpp"

namespace asge::filesystem
{

/**
 * @brief Reads the input file content into a string
 * @param inPath The path of the file
 */
Result<str::String> ReadText( Path const& inPath );

/**
 * @brief Writes (overwriting) the input content into the given file
 * @param inPath The path of the file
 * @param inContent The content to write
 */
BoolResult WriteText( Path const& inPath, str::String const& inContent );

/**
 * @brief Copies (overwriting an existing destination) a file
 * @param inSource The path of the file to copy from
 * @param inDestination The path of the file to copy to
 */
// Named Copy (not CopyFile) — Windows headers #define CopyFile to CopyFileA/W.
BoolResult Copy( Path const& inSource, Path const& inDestination ) noexcept;

}
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

}
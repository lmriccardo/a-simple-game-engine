#pragma once

#include <fstream>
#include <string>
#include <filesystem>

namespace asge::filesystem
{

/**
 * @brief Reads the input file content into a string
 * @param inPath The path of the file
 */
std::string ReadText( std::filesystem::path const& inPath );

}
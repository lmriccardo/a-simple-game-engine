#include "FileIO.hpp"

std::string asge::filesystem::ReadText(std::filesystem::path const &inPath)
{
    std::ifstream file(inPath);
    if (!file.is_open()) return "";

    return {std::istreambuf_iterator<char>(file), 
            std::istreambuf_iterator<char>()};
}
#include "Strings.hpp"

std::string_view asge::str::Trim(std::string_view inSv) noexcept
{
    auto start = inSv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    inSv.remove_prefix(start);

    auto end = inSv.find_last_not_of(" \t\r\n");
    inSv.remove_suffix(inSv.size() - end - 1);

    return inSv;
}

std::vector<std::string> asge::str::Split(std::string_view inSv, const char *inSep)
{
    std::vector<std::string> outVector;
    if (inSv.empty() || inSep == nullptr) return outVector;

    std::size_t startPos{0};
    std::size_t currPos;
    std::size_t sepLen = std::strlen(inSep);

    while ((currPos = inSv.find(inSep, startPos)) != std::string_view::npos)
    {
        // Extract the token up to the delimiter
        outVector.emplace_back(inSv.substr(startPos, currPos - startPos));
        
        // Move the start position past the delimiter
        startPos = currPos + sepLen;
    }

    if (startPos <= inSv.size())
    {
        outVector.emplace_back(inSv.substr(startPos));
    }

    return outVector;
}

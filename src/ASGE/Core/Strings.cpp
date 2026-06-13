#include "Strings.hpp"

using namespace asge::str;

StringView asge::str::Trim(StringView inSv) noexcept
{
    auto start = inSv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    inSv.remove_prefix(start);

    auto end = inSv.find_last_not_of(" \t\r\n");
    inSv.remove_suffix(inSv.size() - end - 1);

    return inSv;
}

std::vector<String> asge::str::Split(StringView inSv, const char *inSep)
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

String asge::str::EncodeUTF8(std::uint32_t inCp) noexcept
{
    std::string out;
    if (inCp < 0x80)
        out += static_cast<char>(inCp);
    else if (inCp < 0x800) {
        out += static_cast<char>(0xC0 | (inCp >> 6));
        out += static_cast<char>(0x80 | (inCp & 0x3F));
    }
    else if (inCp < 0x10000) {
        out += static_cast<char>(0xE0 | (inCp >> 12));
        out += static_cast<char>(0x80 | ((inCp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (inCp & 0x3F));
    }
    else {
        out += static_cast<char>(0xF0 | (inCp >> 18));
        out += static_cast<char>(0x80 | ((inCp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((inCp >> 6)  & 0x3F));
        out += static_cast<char>(0x80 | (inCp & 0x3F));
    }
    return out;
}

String asge::str::ToUTF8(U8String const &inStr) noexcept
{
    return String(reinterpret_cast<const char*>(inStr.data()), inStr.size());
}

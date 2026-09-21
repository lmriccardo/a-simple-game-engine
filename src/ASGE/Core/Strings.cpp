#include "Strings.hpp"

using namespace asge::str;

String asge::str::ToString(TextAlign inAlign) noexcept
{
    switch (inAlign)
    {
        case TextAlign::Left:   return "left";
        case TextAlign::Center: return "center";
        case TextAlign::Right:  return "right";
    }
    return "none";
}

TextAlign asge::str::FromString(StringView inStr) noexcept
{
    if (inStr == "left")   return TextAlign::Left;
    if (inStr == "center") return TextAlign::Center;
    if (inStr == "right")  return TextAlign::Right;

    return TextAlign::None;
}

StringView asge::str::Trim(StringView inSv) noexcept
{
    return Trim( inSv, " \t\r\n" );
}

StringView asge::str::Trim(StringView inSv, StringView inCharsToTrim) noexcept
{
    auto start = inSv.find_first_not_of(inCharsToTrim);
    if (start == std::string_view::npos) return {};
    inSv.remove_prefix(start);

    auto end = inSv.find_last_not_of(inCharsToTrim);
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

std::size_t asge::str::CodePointLength(StringView inStr) noexcept
{
    std::size_t count{0};

    for ( std::size_t ii = 0; ii < inStr.size();)
    {
        unsigned char const c = static_cast<unsigned char>( inStr[ii] );

        // UTF-8 leading byte tells the code point's total byte length:
        // 0xxxxxxx = 1 byte, 110xxxxx = 2 bytes, 1110xxxx = 3 bytes,
        // 11110xxx = 4 bytes. Anything else is malformed input, treat it
        // as 1 byte to avoid infinite loop.
        std::size_t len{1};
        if      ((c & 0x80) == 0x00) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;

        ii += len;
        ++count;
    }

    return count;
}

String asge::str::Justify(
    StringView inStr, TextAlign inAlignment, std::size_t inWidth) noexcept
{
    std::size_t const actualSize = CodePointLength( inStr );
    if ( inWidth <= actualSize ) return String(inStr);
    std::size_t const totalPad = inWidth - actualSize;

    switch ( inAlignment )
    {
    case TextAlign::Left:  return String(inStr) + String( totalPad, ' ' );
    case TextAlign::Right: return String( totalPad, ' ' ) + String(inStr);
    case TextAlign::Center:
    {
        std::size_t const leftPad  = totalPad / 2;
        std::size_t const rightPad = totalPad - leftPad;
        return String(leftPad, ' ') + String(inStr) + String(rightPad, ' ');
    }
    default: return String(inStr);
    }
}

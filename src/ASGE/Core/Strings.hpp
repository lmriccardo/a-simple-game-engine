#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstring>
#include <cstdint>

namespace asge::str
{

using String = std::string;
using StringRef = String&;
using StringCRef = String const&;
using StringView = std::string_view;

using WString = std::wstring;
using WStringView = std::wstring_view;

using U8String = std::u8string;

/** @brief How Justify pads a string to a target width; None pads nothing (see Justify's default case). */
enum class TextAlign
{
    None = 0,
    Left,
    Center,
    Right
};

/**
 * @brief TextAlign -> its lowercase TOML/display representation.
 *
 * Gives "left", "center" or "right", and "none" for TextAlign::None or any
 * unrecognized value. FromString is the inverse.
 */
str::String ToString( TextAlign inAlign ) noexcept;

/**
 * @brief The read-side counterpart to ToString.
 *
 * Any string other than "left", "center" or "right" maps to TextAlign::None,
 * so a missing or misspelled value is never an error. The match is
 * case-sensitive.
 */
TextAlign FromString( StringView inStr ) noexcept;

/**
 * @brief Removes leading and trailing whitespace from a string_view.
 *
 * Strips standard whitespace (spaces, tabs, newlines, carriage returns) from
 * both ends. It returns a view into inSv, so nothing is allocated or copied
 * and the result is only valid as long as the underlying string is.
 */
StringView Trim(StringView inSv) noexcept;

/**
 * @brief Removes leading and trailing characters from a custom set.
 *
 * Same zero-copy behaviour as the single-argument @c Trim, but strips only the
 * characters listed in @p inCharsToTrim instead of standard whitespace.
 */
StringView Trim( StringView inSv, StringView inCharsToTrim ) noexcept;

/**
 * @brief Splits a string_view into owned tokens on a delimiter string.
 *
 * Splits wherever the whole sequence inSep occurs; the delimiter itself is
 * dropped and consecutive delimiters give empty tokens. The tokens are
 * copied into std::strings, so they outlive inSv. An empty inSv or a null
 * inSep gives an empty vector.
 */
std::vector<String> Split( StringView inSv, const char* inSep );

/**
 * @brief UTF-8 encoder for code points such as those written as 4 or 8 hex digit escapes.
 *
 * Encodes inCp into 1 to 4 bytes. Used by the TOML parser to expand
 * unicode escapes in strings.
 */
String EncodeUtf8( std::uint32_t inCp ) noexcept;

/**
 * @brief Converts a std::u8string into a plain std::string with the same bytes.
 *
 * A reinterpretation of the bytes, no transcoding: char8_t and char hold the
 * same UTF-8 sequence afterwards.
 */
String ToUtf8( U8String const& inStr ) noexcept;

/**
 * @brief Counts inStr's Unicode code points rather than its bytes.
 *
 * Walks inStr one UTF-8 sequence at a time via each byte's leading-byte
 * pattern; a byte that doesn't match a valid 1-4 byte UTF-8 lead is treated
 * as its own 1-byte code point (malformed input still terminates instead
 * of looping forever, at the cost of an inflated count for that byte).
 */
std::size_t CodePointLength( StringView inStr ) noexcept;

/**
 * @brief Decodes one UTF-8 code point at ioPos and advances ioPos past it.
 *
 * ioPos is updated by reference to just after the consumed bytes, so loops
 * must not increment it again. Malformed bytes decode as U+FFFD (and still
 * advance), which keeps such loops terminating.
 */
int DecodeUtf8( StringView inText, std::size_t& ioPos ) noexcept;

/**
 * @brief Pads inStr with spaces to inWidth code points, per inAlignment.
 *
 * Returns inStr unchanged (no truncation) if it's already at least inWidth
 * code points long. TextAlign::Center splits an odd remainder with the
 * extra space on the right; TextAlign::None pads nothing, same as an
 * already-wide-enough string.
 */
String Justify( StringView inStr, TextAlign inAlignment, std::size_t inWidth ) noexcept;

}
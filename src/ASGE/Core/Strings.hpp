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

/** @brief How Justify pads a string to a target width — None pads nothing (see Justify's default case). */
enum class TextAlign
{
    None = 0,
    Left,
    Center,
    Right
};

/** @brief TextAlign -> its lowercase TOML/display representation ("none" for anything unrecognized). */
str::String ToString( TextAlign inAlign ) noexcept;

/** @brief The read-side counterpart to ToString — any string other than "left"/"center"/"right" maps to None. */
TextAlign FromString( StringView inStr ) noexcept;

/**
 * @brief Removes leading and trailing whitespace from a string_view.
 *
 * This function scans the input string_view from both ends, stripping out standard 
 * whitespace characters (spaces, tabs, newlines, and carriage returns). Because it 
 * operates on and returns a `std::string_view`, it performs no dynamic memory 
 * allocations or string copying.
 *
 * @param inSv The input string_view to be trimmed.
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
 * @brief Splits a string_view into a vector of sub-views based on a delimiter string.
 *
 * Scans the input string_view and splits it wherever the sequence of characters 
 * specified by the delimiter `inSep` is found. Because it returns a vector of 
 * `std::string_view`, the individual tokens are zero-copy views into the original string.
 *
 * @param inSv  The input string_view to be split.
 * @param inSep A null-terminated C-string representing the delimiter sequence.
 * @return A `std::vector<std::string>` containing the split tokens in order.
 */
std::vector<String> Split( StringView inSv, const char* inSep );

/**
 * @brief UTF-8 encoder for \uXXXX and \UXXXXXXXX
 * @param inCp The unsigned 32-bit integer to encode into a string
 * @return The encoded string
 */
String EncodeUTF8( std::uint32_t inCp ) noexcept;

/**
 * @brief Converts UTF-8 string into a simple string
 */
String ToUTF8( U8String const& inStr ) noexcept;

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
 * @brief Pads inStr with spaces to inWidth code points, per inAlignment.
 *
 * Returns inStr unchanged (no truncation) if it's already at least inWidth
 * code points long. TextAlign::Center splits an odd remainder with the
 * extra space on the right; TextAlign::None pads nothing, same as an
 * already-wide-enough string.
 */
String Justify( StringView inStr, TextAlign inAlignment, std::size_t inWidth ) noexcept;

}
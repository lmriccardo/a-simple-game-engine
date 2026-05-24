#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstring>
#include <cstdint>

namespace asge::str
{

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
std::string_view Trim(std::string_view inSv) noexcept;

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
std::vector<std::string> Split( std::string_view inSv, const char* inSep );

/**
 * @brief UTF-8 encoder for \uXXXX and \UXXXXXXXX
 * @param inCp The unsigned 32-bit integer to encode into a string
 * @return The encoded string
 */
std::string EncodeUTF8( std::uint32_t inCp ) noexcept;

}
#pragma once

#include <cstdint>

namespace asge::graphics
{

/** @brief An 8-bit-per-channel RGBA color, defaulting to opaque white. */
struct RGBA_Color
{
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};
};

/** @brief A small named palette of RGBA_Color constants, covering basics, grays, primaries/secondaries, UI button states, and overlay colors. */
namespace colors
{

// Basics
inline static constexpr RGBA_Color s_White                  { 255, 255, 255, 255 };
inline static constexpr RGBA_Color s_Black                  {   0,   0,   0, 255 };
inline static constexpr RGBA_Color s_Transparent            {   0,   0,   0,   0 };

// Grays
inline static constexpr RGBA_Color s_LightGray              { 211, 211, 211, 255 };
inline static constexpr RGBA_Color s_Gray                   { 128, 128, 128, 255 };
inline static constexpr RGBA_Color s_DarkGray               {  64,  64,  64, 255 };

// Primaries and secondaries
inline static constexpr RGBA_Color s_Red                    { 230,  41,  55, 255 };
inline static constexpr RGBA_Color s_Green                  {   0, 200,  83, 255 };
inline static constexpr RGBA_Color s_Blue                   {   0, 121, 241, 255 };
inline static constexpr RGBA_Color s_Yellow                 { 253, 216,  53, 255 };
inline static constexpr RGBA_Color s_Orange                 { 255, 152,   0, 255 };
inline static constexpr RGBA_Color s_Purple                 { 156,  39, 176, 255 };
inline static constexpr RGBA_Color s_Cyan                   {   0, 188, 212, 255 };
inline static constexpr RGBA_Color s_Magenta                { 233,  30,  99, 255 };
inline static constexpr RGBA_Color s_Brown                  { 121,  85,  72, 255 };

// UI (normal / hover / pressed triples)
inline static constexpr RGBA_Color s_ButtonLight          = s_White;
inline static constexpr RGBA_Color s_ButtonLightHover       { 230, 230, 230, 255 };
inline static constexpr RGBA_Color s_ButtonLightPressed     { 200, 200, 200, 255 };

inline static constexpr RGBA_Color s_ButtonDark             {  45,  45,  48, 255 };
inline static constexpr RGBA_Color s_ButtonDarkHover        {  62,  62,  66, 255 };
inline static constexpr RGBA_Color s_ButtonDarkPressed      {  30,  30,  32, 255 };

inline static constexpr RGBA_Color s_ButtonAccent           {   0, 121, 241, 255 };
inline static constexpr RGBA_Color s_ButtonAccentHover      {  30, 144, 255, 255 };
inline static constexpr RGBA_Color s_ButtonAccentPressed    {   0,  90, 180, 255 };

// Overlays
inline static constexpr RGBA_Color s_ShadowBlack            {   0,   0,   0, 128 };   // 50% black, for panels behind HUD text

}

}
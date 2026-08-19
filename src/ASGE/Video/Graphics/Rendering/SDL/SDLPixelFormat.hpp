#pragma once

#include <SDL3/SDL.h>
#include <ASGE/Core/Graphics/PixelFormat.hpp>
#include <ASGE/Core/Graphics/Image.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace asge::video
{

/**
 * @brief Maps an asge::graphics::PixelFormat to the SDL_PixelFormat SDLTexture uploads as
 */
[[nodiscard]] SDL_PixelFormat MapPixelFormat( graphics::PixelFormat inFormat ) noexcept;

// Tightly-packed pixel bytes ready to hand to SDL_UpdateTexture, plus their stride.
struct SDLUploadBuffer
{
    std::vector<std::uint8_t> s_Data;
    std::size_t s_Stride{0};
};

/**
 * @brief Converts an Image's pixel bytes into the layout MapPixelFormat's return value expects
 *
 * RGBA8 images pass through unchanged. A8 images are expanded one byte into
 * four: RGB set to white, alpha set to the source byte -- the standard trick
 * for uploading single-channel glyph/mask data as a normal alpha-blended
 * RGBA texture, tinted via SDL_SetTextureColorMod by the caller.
 */
[[nodiscard]] SDLUploadBuffer ExpandPixelsForUpload( graphics::Image const& inImage );

}

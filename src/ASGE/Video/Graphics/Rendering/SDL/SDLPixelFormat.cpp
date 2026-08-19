#include "SDLPixelFormat.hpp"

SDL_PixelFormat asge::video::MapPixelFormat( graphics::PixelFormat inFormat ) noexcept
{
    switch (inFormat)
    {
    case graphics::PixelFormat::RGBA8: return SDL_PIXELFORMAT_RGBA32;
    case graphics::PixelFormat::A8:    return SDL_PIXELFORMAT_RGBA32; // expanded, see ExpandPixelsForUpload
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}

asge::video::SDLUploadBuffer asge::video::ExpandPixelsForUpload( graphics::Image const &inImage )
{
    auto const dims = inImage.Dimensions();
    auto const width = dims.x() > 0 ? static_cast<std::size_t>(dims.x()) : 0;
    auto const height = dims.y() > 0 ? static_cast<std::size_t>(dims.y()) : 0;
    auto const* src = inImage.Data();

    switch (inImage.Format())
    {
    case graphics::PixelFormat::RGBA8:
    {
        auto const stride = inImage.Stride();
        return { std::vector<std::uint8_t>(src, src + stride * height), stride };
    }
    case graphics::PixelFormat::A8:
    {
        std::vector<std::uint8_t> data(width * height * 4);
        for (std::size_t i = 0; i < width * height; ++i)
        {
            data[i * 4 + 0] = 255; // R
            data[i * 4 + 1] = 255; // G
            data[i * 4 + 2] = 255; // B
            data[i * 4 + 3] = src[i]; // A -- the actual single-channel source byte
        }
        return { std::move(data), width * 4 };
    }
    }

    return {};
}

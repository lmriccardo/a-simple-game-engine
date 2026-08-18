#pragma once

#include <SDL3/SDL.h>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Graphics/Image.hpp>

namespace asge::video
{

/* RAII SDL-backed Texture Implementation */
class SDLTexture final : public ITexture
{
private:
    SDL_Texture* m_Handle{nullptr}; // The SDL native handle for textures
public:
    SDLTexture(SDL_Renderer* inRenderer, graphics::Image const& inImage);
    SDLTexture(SDLTexture&& inOther);
    SDLTexture& operator=(SDLTexture&& inOther);

    virtual ~SDLTexture();

    SDLTexture(SDLTexture const&) = delete;
    SDLTexture& operator=(SDLTexture const&) = delete;

    [[nodiscard]] math::Int2 Size() const noexcept override;
    [[nodiscard]] void* NativeHandle() const noexcept override;
    [[nodiscard]] bool IsValid() const noexcept override;

    void Destroy();
};

}
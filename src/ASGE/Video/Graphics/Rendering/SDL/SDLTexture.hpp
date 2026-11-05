#pragma once

#include <SDL3/SDL.h>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Media/Image.hpp>

namespace asge::video
{

/**
 * @brief RAII SDL-backed ITexture implementation.
 */
class SDLTexture final : public ITexture
{
private:
    SDL_Texture* m_Handle{nullptr}; // The SDL native handle for textures
public:
    /**
     * @brief Uploads inImage as a new SDL texture owned by this object.
     *
     * The image is converted with ExpandPixelsForUpload first. If texture
     * creation or the upload fails, the error is logged and the texture is
     * destroyed, so IsValid() is false instead of reporting an empty texture.
     */
    SDLTexture(SDL_Renderer* inRenderer, media::Image const& inImage);

    /**
     * @brief Takes ownership of inOther's SDL texture, leaving it empty.
     */
    SDLTexture(SDLTexture&& inOther);

    /**
     * @brief Move-assigns inOther's SDL texture, leaving inOther empty.
     *
     * Self-assignment is a no-op. Unlike SDLWindow/SDLRenderer it does NOT
     * destroy the texture currently held first, so that one is leaked.
     */
    SDLTexture& operator=(SDLTexture&& inOther);

    /**
     * @brief Destroys the SDL texture (see Destroy()).
     */
    virtual ~SDLTexture();

    /**
     * @brief Copy construction is disabled.
     */
    SDLTexture(SDLTexture const&) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    SDLTexture& operator=(SDLTexture const&) = delete;

    /**
     * @brief Texture size from the SDL texture; {0, 0} when empty.
     */
    [[nodiscard]] math::Int2 Size() const noexcept override;

    /**
     * @brief Returns the owned SDL_Texture* as a void* (null when empty).
     */
    [[nodiscard]] void* NativeHandle() const noexcept override;

    /**
     * @brief True while the native SDL texture exists.
     */
    [[nodiscard]] bool IsValid() const noexcept override;

    /**
     * @brief Sets SDL's color and alpha mod from inColor.
     *
     * Two SDL calls (color then alpha); each failure is logged separately.
     */
    void SetColorMod( graphics::RGBA_Color inColor ) noexcept override;

    /**
     * @brief Reads SDL's color and alpha mod back.
     *
     * Returns an error Result carrying SDL's message if either query fails.
     */
    Result<graphics::RGBA_Color> GetColorMod() const noexcept override;

    /**
     * @brief Destroys the native SDL texture.
     *
     * Safe to call more than once; afterwards IsValid() is false.
     */
    void Destroy();
};

}
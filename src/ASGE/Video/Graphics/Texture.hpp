#pragma once

#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::video
{
    
class ITexture
{
public:
    /**
     * @brief Virtual destructor so derived types are destroyed through the interface.
     */
    virtual ~ITexture() = default;

    /**
     * @brief Copy construction is disabled.
     */
    ITexture(ITexture const&) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    ITexture& operator=(ITexture const&) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    ITexture(ITexture&&) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    ITexture& operator=(ITexture&&) = default;

    /**
     * @brief Texture size in pixels.
     *
     * Returns {0, 0} for an invalid texture.
     */
    [[nodiscard]] virtual math::Int2 Size() const noexcept = 0;

    /**
     * @brief Backend-native texture handle (e.g. SDL_Texture*).
     *
     * The texture keeps ownership. Null once the texture is destroyed.
     */
    [[nodiscard]] virtual void* NativeHandle() const noexcept = 0;

    /**
     * @brief True while the texture holds uploaded pixel data.
     *
     * False after a failed upload or after the texture was moved from.
     */
    [[nodiscard]] virtual bool IsValid() const noexcept = 0;

    /**
     * @brief Sets the color and alpha multiplied into every draw of this texture.
     *
     * inColor's RGB tints the pixels and its alpha scales opacity; white at
     * full alpha is a no-op. Applies to all later draws, so the font atlas
     * is re-tinted per DrawString call. Backend failures are logged.
     */
    virtual void SetColorMod( graphics::RGBA_Color inColor ) noexcept = 0;

    /**
     * @brief Reads back the color and alpha set by SetColorMod.
     *
     * Returns an error Result if the backend query fails (e.g. on an invalid
     * texture) instead of a color.
     */
    virtual Result<graphics::RGBA_Color> GetColorMod() const noexcept = 0;

protected:
    /**
     * @brief Default constructor.
     *
     * Defaulted; members take their default values.
     */
    ITexture() = default;
};

}
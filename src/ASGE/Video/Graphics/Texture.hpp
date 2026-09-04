#pragma once

#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Media/Color.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::video
{
    
class ITexture
{
public:
    virtual ~ITexture() = default;

    ITexture(ITexture const&) = delete;
    ITexture& operator=(ITexture const&) = delete;
    ITexture(ITexture&&) = default;
    ITexture& operator=(ITexture&&) = default;

    [[nodiscard]] virtual math::Int2 Size() const noexcept = 0;
    [[nodiscard]] virtual void* NativeHandle() const noexcept = 0;
    [[nodiscard]] virtual bool IsValid() const noexcept = 0;

    virtual void SetColorMod( media::RGBA_Color inColor ) noexcept = 0;
    virtual Result<media::RGBA_Color> GetColorMod() const noexcept = 0;

protected:
    ITexture() = default;
};

}
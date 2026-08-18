#pragma once

#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

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

protected:
    ITexture() = default;
};

}
#pragma once

#include <random>
#include <ASGE/ASGE.hpp>

namespace asge_g = asge::media;

class BackgroundComponent
{
private:
    using color_type = asge_g::RGBA_Color;
    color_type m_Color;
public:
    BackgroundComponent( color_type const& initColor )
        : m_Color( initColor )
    {}
    
    color_type const& GetColor() const noexcept;
    static color_type GetRandomColor() noexcept;
    void SetColor( color_type const& inColor );
};
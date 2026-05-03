#include "Component.hpp"

void BackgroundComponent::SetColor(color_type const &inColor)
{
    m_Color = inColor;
    LOG_INFO( "New background color has been set" );
}

BackgroundComponent::color_type const &BackgroundComponent::GetColor() const noexcept
{
    return m_Color;
}

BackgroundComponent::color_type BackgroundComponent::GetRandomColor() noexcept
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dist(0, 255);

    return color_type{
        static_cast<uint8_t>(dist(gen)),
        static_cast<uint8_t>(dist(gen)),
        static_cast<uint8_t>(dist(gen)),
        255
    };
}

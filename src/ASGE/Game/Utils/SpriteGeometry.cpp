#include "SpriteGeometry.hpp"

namespace
{

std::optional<asge::math::Rect> SpriteRectAt(
    asge::game::components::Sprite const& inSprite, asge::math::Float2 inPos, asge::math::Float2 inScale ) noexcept
{
    using namespace asge;
    if ( !inSprite.m_Texture ) return std::nullopt;

    auto const& texture = *inSprite.m_Texture;
    auto const& srcRect = inSprite.m_SourceRect;
    float srcW{}, srcH{};

    if ( srcRect.has_value() )
    {
        srcW = srcRect->m_Width;
        srcH = srcRect->m_Height;
    }
    else
    {
        math::Int2 const texSize = texture.Size();
        srcW = static_cast<float>(texSize.x());
        srcH = static_cast<float>(texSize.y());
    }

    return math::Rect{ inPos.x(), inPos.y(), srcW * inScale.x(), srcH * inScale.y() };
}

}

std::optional<asge::math::Rect> asge::game::utils::SpriteGetDstRect(
    components::Sprite const& inSprite, components::Transform const& inT ) noexcept
{
    return SpriteRectAt( inSprite, inT.m_WorldCoordinates, inT.m_WorldScale );
}

std::optional<asge::math::Rect> asge::game::utils::SpriteGetLocalRect(
    components::Sprite const& inSprite, components::Transform const& inT ) noexcept
{
    return SpriteRectAt( inSprite, inT.m_LocalCoordinates, inT.m_LocalScale );
}

void asge::game::utils::SpriteScaleAroundCenter(
    components::Sprite const& inSprite, components::Transform& ioT, math::Float2 inNewScale ) noexcept
{
    auto const rect = SpriteGetLocalRect( inSprite, ioT );
    if ( !rect ) return;

    // Native (unscaled) size, recovered from the current local rect; a zero scale can't be un-scaled.
    float const oldSx = ioT.m_LocalScale.x(), oldSy = ioT.m_LocalScale.y();
    if ( oldSx == 0.0f || oldSy == 0.0f ) return;

    float const cx = rect->m_X + rect->m_Width  * 0.5f;
    float const cy = rect->m_Y + rect->m_Height * 0.5f;
    float const nativeW = rect->m_Width  / oldSx;
    float const nativeH = rect->m_Height / oldSy;

    ioT.m_LocalScale       = inNewScale;
    ioT.m_LocalCoordinates = { cx - nativeW * inNewScale.x() * 0.5f, cy - nativeH * inNewScale.y() * 0.5f };
    ioT.m_Dirty            = true;
}

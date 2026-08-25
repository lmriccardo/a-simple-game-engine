#include "RenderSystem.hpp"

void asge::game::systems::RenderSystem(ecs::Registry &inRegistry, video::IRenderer &inRenderer) noexcept
{
    for ( auto [ entity, transform, sprite ]
            : inRegistry.View<components::Transform, components::Sprite>() )
    {
        (void)entity;

        video::ITexture* texture = sprite.get().m_Texture;
        if ( !texture ) continue;

        auto const& t = transform.get();
        auto const& src = sprite.get().m_SourceRect;

        float srcW{};
        float srcH{};

        if ( src.has_value() )
        {
            srcW = src->w;
            srcH = src->h;
        }
        else
        {
            math::Int2 const texSize = texture->Size();
            srcW = static_cast<float>(texSize.x());
            srcH = static_cast<float>(texSize.y());
        }

        math::Rect const destRect{
            t.m_X, t.m_Y,
            srcW * t.m_ScaleX,
            srcH * t.m_ScaleY
        };

        if ( src.has_value() )
        {
            inRenderer.DrawTexture( *texture, *src, destRect );
        }
        else
        {
            inRenderer.DrawTexture( *texture, destRect );
        }
    }
}

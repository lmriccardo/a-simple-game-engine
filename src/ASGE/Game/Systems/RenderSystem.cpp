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
        math::Int2 const texSize = texture->Size();
        math::Rect const destRect{
            t.m_X, t.m_Y,
            static_cast<float>(texSize.x()) * t.m_ScaleX,
            static_cast<float>(texSize.y()) * t.m_ScaleY
        };

        if ( auto const& src = sprite.get().m_SourceRect; src.has_value() )
        {
            inRenderer.DrawTexture( *texture, *src, destRect );
        }
        else
        {
            inRenderer.DrawTexture( *texture, destRect );
        }
    }
}

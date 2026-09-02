#include "RenderSystem.hpp"

#include <vector>
#include <algorithm>

namespace
{

using namespace asge;

/** @brief One drawable entity's precomputed sort keys and destination rect for a single frame. */
struct DrawItem
{
    ecs::Entity                        m_Entity;    // Source entity; index is the tie-break of last resort
    game::components::Transform const* m_Transform;
    game::components::Sprite const*    m_Sprite;
    int                                m_Layer;     // Copied from Sprite::m_Layer
    float                              m_SortY;     // Bottom edge (position.y + drawn height), for y-sort
    bool                               m_YSort;     // Copied from Sprite::m_YSort
    math::Rect                         m_DstRect;
};

/** @brief Orders DrawItems by layer, then bottom-edge Y when either side opts into y-sort, then entity index. */
bool operator<(DrawItem const& a, DrawItem const& b) noexcept
{
    if (a.m_Layer != b.m_Layer) return a.m_Layer < b.m_Layer;
    if (a.m_YSort || b.m_YSort)
    {
        if (a.m_SortY != b.m_SortY) return a.m_SortY < b.m_SortY;
    }
    return a.m_Entity.m_Index < b.m_Entity.m_Index;
}

/** @brief Builds a DrawItem from an entity's Transform + Sprite, or nullopt if the sprite has no texture. */
std::optional<DrawItem> ConstructFrom(
    ecs::Entity inE, game::components::Transform const& inT,
    game::components::Sprite const& inS
) noexcept {
    auto const& result = game::components::SpriteGetDstRect( inS, inT );
    if ( !result.has_value() ) return std::nullopt;
    return DrawItem
    {
        inE, &inT, &inS, inS.m_Layer, inT.m_Y + (*result).h,
        inS.m_YSort, *result
    };
}

}

void asge::game::systems::RenderSystem(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer) noexcept
{
    std::vector<DrawItem> drawItems;

    for ( auto [ entity, transform, sprite ]
            : inRegistry.View<components::Transform, components::Sprite>() )
    {
        if ( auto item = ConstructFrom( entity, transform.get(), sprite.get() ); item.has_value() )
        {
            drawItems.push_back( *item );
        }
    }

    std::sort( drawItems.begin(), drawItems.end());

    for ( auto const& drawItem : drawItems )
    {
        video::ITexture* texture = drawItem.m_Sprite->m_Texture;
        auto const& src = drawItem.m_Sprite->m_SourceRect;

        if ( src.has_value() )
        {
            inRenderer.DrawTexture( *texture, *src, drawItem.m_DstRect );
        }
        else
        {
            inRenderer.DrawTexture( *texture, drawItem.m_DstRect );
        }
    }
}

#include "RenderSystem.hpp"

#include <vector>
#include <algorithm>
#include <cmath>

#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Resources/ActiveCamera.hpp>
#include <ASGE/Game/Components/Camera.hpp>
#include <ASGE/Video/Graphics/Camera.hpp>
#include <ASGE/Core/Math/Geometry/Collision.hpp>

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
        inE, &inT, &inS, inS.m_Layer, inT.m_Y + (*result).m_Height,
        inS.m_YSort, *result
    };
}

}

void asge::game::systems::AnimationSystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [ entity, sprite, animation ]
            : inRegistry.View<components::Sprite, components::Animation>() )
    {
        auto& animationRef = animation.get();
        auto& spriteRef = sprite.get();

        if ( animationRef.m_ClipPath.empty() || !animationRef.m_Clip ) continue;

        std::vector<math::Rect> const& frames = animationRef.m_Clip->Get().m_Frames;

        if ( !animationRef.m_Playing || frames.empty() || !spriteRef.m_Texture
             || animationRef.m_FrameDuration <= 0.0f )
        {
            continue;
        }

        animationRef.m_ElapsedTime += inDeltaTime;
        auto const nofFrames = frames.size();
        while ( animationRef.m_ElapsedTime >= animationRef.m_FrameDuration )
        {
            animationRef.m_ElapsedTime -= animationRef.m_FrameDuration;
            ++animationRef.m_CurrentFrame;
            if ( animationRef.m_CurrentFrame >= nofFrames )
            {
                animationRef.m_CurrentFrame = animationRef.m_Loop ? 0 : nofFrames - 1;
                if ( !animationRef.m_Loop ) animationRef.m_Playing = false;
            }
        }

        spriteRef.m_SourceRect = frames[animationRef.m_CurrentFrame];
    }
}

void asge::game::systems::CameraSystem(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer, float inDeltaTime) noexcept
{
    auto active = inRegistry.GetResource<resources::ActiveCamera>();
    if ( !active || active.Value().get().m_Entity == ecs::Entity::Null() ) return;

    auto const& entity = active.Value().get().m_Entity;
    auto cameraResult = inRegistry.GetComponent<components::Camera>( entity );
    auto transformResult = inRegistry.GetComponent<components::Transform>( entity );
    if ( !cameraResult || !transformResult ) return;

    auto& cameraComp = cameraResult.Value().get();
    auto& transform  = transformResult.Value().get();
    video::Camera camera = inRenderer.GetCamera();
    camera.m_Zoom = cameraComp.m_Zoom;

    float const targetX = transform.m_X - inRenderer.GetViewport().m_Width  / ( 2.0f * camera.m_Zoom );
    float const targetY = transform.m_Y - inRenderer.GetViewport().m_Height / ( 2.0f * camera.m_Zoom );

    if ( cameraComp.m_Smoothing <= 0.0f )
    {
        camera.m_X = targetX;
        camera.m_Y = targetY;
    }
    else
    {
        float const k = 1.0f - std::exp( -cameraComp.m_Smoothing * inDeltaTime );
        camera.m_X += ( targetX - camera.m_X ) * k;
        camera.m_Y += ( targetY - camera.m_Y ) * k;
    }

    inRenderer.SetCamera( camera );
}

void asge::game::systems::RenderSystem(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer) noexcept
{
    std::vector<DrawItem> drawItems;
    math::Rect const visible = video::VisibleWorldRect( inRenderer.GetCamera(), inRenderer.GetViewport() );

    for ( auto [ entity, transform, sprite ]
            : inRegistry.View<components::Transform, components::Sprite>() )
    {
        if ( auto item = ConstructFrom( entity, transform.get(), sprite.get() ); item.has_value() )
        {
            if ( !math::AabbOverlap( item->m_DstRect, visible ) ) continue;
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

void asge::game::systems::RenderPipeline(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer, float inDeltaTime) noexcept
{
    AnimationSystem( inRegistry, inDeltaTime );
    CameraSystem( inRegistry, inRenderer, inDeltaTime );
    RenderSystem( inRegistry, inRenderer );
}

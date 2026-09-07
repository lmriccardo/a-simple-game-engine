#include "PhysicsDebugSystem.hpp"
#include "PhysicsSystem.hpp"

#include <ASGE/Core/Media/Color.hpp>

namespace
{

/** @brief The outline color DebugDrawColliders uses for one ResolutionType. */
asge::media::RGBA_Color ColorForResolution( asge::game::components::ResolutionType inResolution ) noexcept
{
    using asge::game::components::ResolutionType;
    switch ( inResolution )
    {
    case ResolutionType::Solid:   return { 0, 255, 0, 255 };
    case ResolutionType::Trigger: return { 255, 255, 0, 255 };
    case ResolutionType::Unknown: return { 128, 128, 128, 255 };
    }
    return { 128, 128, 128, 255 };
}

}

void asge::game::systems::DebugDrawColliders( ecs::Registry& inRegistry, video::IRenderer& inRenderer ) noexcept
{
    for ( auto [ entity, transform, collider ]
            : inRegistry.View<components::Transform, components::Collider>() )
    {
        (void)entity;

        auto const bounds = WorldBounds( transform.get(), collider.get() );
        auto const color  = ColorForResolution( collider.get().m_Resolution );

        std::visit( [&inRenderer, &color]( auto const& inShape )
        {
            using ShapeT = std::decay_t<decltype(inShape)>;

            if constexpr ( std::is_same_v<ShapeT, math::Rect> )
            {
                inRenderer.DrawRect( inShape, color, false );
            }
            else
            {
                inRenderer.DrawCircle(
                    math::Int2{
                        static_cast<int>( inShape.m_Center.x() ),
                        static_cast<int>( inShape.m_Center.y() )
                    },
                    static_cast<int>( inShape.m_Radius ),
                    color, false
                );
            }
        }, bounds );
    }
}

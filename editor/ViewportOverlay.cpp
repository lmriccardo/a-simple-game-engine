#include "ViewportOverlay.hpp"

#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Collider.hpp>

#include <imgui.h>

#include <cmath>
#include <variant>

using namespace asge::game::components;

namespace
{

ImVec2 ToImVec2( asge::math::Float2 inV ) noexcept
{
    return { inV.x(), inV.y() };
}

}

void DrawWorldGrid( asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList, float inSpacing ) noexcept
{
    if ( inSpacing <= 0.0f ) return; // guards the loops below against a degenerate/non-positive spacing

    constexpr ImU32 kLineColor = IM_COL32( 255, 255, 255, 30 );
    constexpr ImU32 kAxisColor = IM_COL32( 255, 90, 90, 160 ); // brighter for the X=0/Y=0 axes

    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();
    auto const visible = asge::video::VisibleWorldRect( camera, viewport );

    float const firstX = std::floor( visible.m_X / inSpacing ) * inSpacing;
    for ( float x = firstX; x <= visible.m_X + visible.m_Width; x += inSpacing )
    {
        auto const p1 = asge::video::WorldToScreen( camera, viewport, asge::math::Float2{ x, visible.m_Y } );
        auto const p2 = asge::video::WorldToScreen(
            camera, viewport, asge::math::Float2{ x, visible.m_Y + visible.m_Height } );
        inDrawList->AddLine( ToImVec2( p1 ), ToImVec2( p2 ), std::abs( x ) < 0.01f ? kAxisColor : kLineColor );
    }

    float const firstY = std::floor( visible.m_Y / inSpacing ) * inSpacing;
    for ( float y = firstY; y <= visible.m_Y + visible.m_Height; y += inSpacing )
    {
        auto const p1 = asge::video::WorldToScreen( camera, viewport, asge::math::Float2{ visible.m_X, y } );
        auto const p2 = asge::video::WorldToScreen(
            camera, viewport, asge::math::Float2{ visible.m_X + visible.m_Width, y } );
        inDrawList->AddLine( ToImVec2( p1 ), ToImVec2( p2 ), std::abs( y ) < 0.01f ? kAxisColor : kLineColor );
    }
}

void DrawColliderOverlays(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry, ImDrawList* inDrawList ) noexcept
{
    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();

    for ( auto entity : inRegistry.AllEntities() )
    {
        auto transformResult = inRegistry.GetComponent<Transform>( entity );
        auto colliderResult = inRegistry.GetComponent<Collider>( entity );
        if ( !transformResult || !colliderResult ) continue;

        auto const& t = transformResult.Value().get();
        auto const& collider = colliderResult.Value().get();

        // Colored by the low 3 bits of m_Layer -- enough to visually tell
        // common single-bit layer setups apart without a full palette.
        ImU32 const fillColor = IM_COL32(
            ( collider.m_Layer & 0x1u ) ? 255 : 70,
            ( collider.m_Layer & 0x2u ) ? 255 : 70,
            ( collider.m_Layer & 0x4u ) ? 255 : 70,
            80 );
        constexpr ImU32 kOutlineColor = IM_COL32( 255, 255, 255, 180 );

        if ( auto const* rect = std::get_if<asge::math::Rect>( &collider.m_LocalBounds ) )
        {
            // Collider's world position is Transform's, offset by the
            // shape's own local origin -- see Collider.hpp's doc comment.
            asge::math::Float2 const worldMin{ t.m_X + rect->m_X, t.m_Y + rect->m_Y };
            asge::math::Float2 const worldMax{ worldMin.x() + rect->m_Width, worldMin.y() + rect->m_Height };
            auto const screenMin = asge::video::WorldToScreen( camera, viewport, worldMin );
            auto const screenMax = asge::video::WorldToScreen( camera, viewport, worldMax );

            inDrawList->AddRectFilled( ToImVec2( screenMin ), ToImVec2( screenMax ), fillColor );
            inDrawList->AddRect( ToImVec2( screenMin ), ToImVec2( screenMax ), kOutlineColor );
        }
        else if ( auto const* circle = std::get_if<asge::math::Circle>( &collider.m_LocalBounds ) )
        {
            asge::math::Float2 const worldCenter{ t.m_X + circle->m_Center.x(), t.m_Y + circle->m_Center.y() };
            auto const screenCenter = asge::video::WorldToScreen( camera, viewport, worldCenter );
            float const screenRadius = circle->m_Radius * camera.m_Zoom;

            inDrawList->AddCircleFilled( ToImVec2( screenCenter ), screenRadius, fillColor );
            inDrawList->AddCircle( ToImVec2( screenCenter ), screenRadius, kOutlineColor );
        }
    }
}

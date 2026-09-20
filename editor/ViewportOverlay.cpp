#include "ViewportOverlay.hpp"

#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Collider.hpp>

#include <imgui.h>

#include <cmath>
#include <optional>
#include <variant>

using namespace asge::game::components;

asge::math::Rect GetEntityWorldBounds(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity, Transform const& inTransform ) noexcept
{
    asge::math::Rect bounds{ inTransform.m_X, inTransform.m_Y, 1.0f, 1.0f };
    if ( auto spriteResult = inRegistry.GetComponent<Sprite>( inEntity ) )
    {
        if ( auto dst = SpriteGetDstRect( spriteResult.Value().get(), inTransform ) ) bounds = *dst;
    }

    // Widened (never shrunk) to at least this size, centered on whatever the
    // real bounds were -- without this, a small sprite (or a point-only
    // entity with none at all) leaves almost nothing to click for picking or
    // starting a free-drag, short of landing exactly on it.
    constexpr float kMinPickSize = 24.0f;
    if ( bounds.m_Width < kMinPickSize )
    {
        bounds.m_X -= ( kMinPickSize - bounds.m_Width ) * 0.5f;
        bounds.m_Width = kMinPickSize;
    }
    if ( bounds.m_Height < kMinPickSize )
    {
        bounds.m_Y -= ( kMinPickSize - bounds.m_Height ) * 0.5f;
        bounds.m_Height = kMinPickSize;
    }

    return bounds;
}

namespace
{

ImVec2 ToImVec2( asge::math::Float2 inV ) noexcept
{
    return { inV.x(), inV.y() };
}

constexpr float kGizmoLength = 28.0f; // screen pixels, fixed regardless of zoom
constexpr float kGizmoHitPadding = 6.0f; // pixels either side of an arm counted as a hit
constexpr ImU32 kGizmoXColor = IM_COL32( 230, 60, 60, 255 );
constexpr ImU32 kGizmoYColor = IM_COL32( 60, 200, 60, 255 );

// Screen-space anchor point for each of the gizmo's four arrows -- the
// midpoint of its own edge on GetEntityWorldBounds, so handles sit beside
// the entity rather than all bunched at its Transform corner.
struct GizmoHandlePoints { ImVec2 east, west, north, south; };

std::optional<GizmoHandlePoints> ComputeGizmoHandles(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected ) noexcept
{
    auto transformResult = inRegistry.GetComponent<Transform>( inSelected );
    if ( !transformResult ) return std::nullopt;

    auto const worldBounds = GetEntityWorldBounds( inRegistry, inSelected, transformResult.Value().get() );

    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();
    auto const screenMin = asge::video::WorldToScreen(
        camera, viewport, asge::math::Float2{ worldBounds.m_X, worldBounds.m_Y } );
    auto const screenMax = asge::video::WorldToScreen( camera, viewport,
        asge::math::Float2{ worldBounds.m_X + worldBounds.m_Width, worldBounds.m_Y + worldBounds.m_Height } );

    float const midX = ( screenMin.x() + screenMax.x() ) * 0.5f;
    float const midY = ( screenMin.y() + screenMax.y() ) * 0.5f;

    return GizmoHandlePoints{
        ImVec2{ screenMax.x(), midY }, // east
        ImVec2{ screenMin.x(), midY }, // west
        ImVec2{ midX, screenMin.y() }, // north
        ImVec2{ midX, screenMax.y() }  // south
    };
}

// Draws a single arrowhead-tipped handle from inOrigin, kGizmoLength along inDir (a unit vector).
void DrawArrow( ImDrawList* inDrawList, ImVec2 inOrigin, ImVec2 inDir, ImU32 inColor ) noexcept
{
    ImVec2 const tip{ inOrigin.x + inDir.x * kGizmoLength, inOrigin.y + inDir.y * kGizmoLength };
    inDrawList->AddLine( inOrigin, tip, inColor, 2.5f );

    constexpr float kHeadSize = 6.0f;
    ImVec2 const perp{ -inDir.y, inDir.x };
    ImVec2 const back{ tip.x - inDir.x * kHeadSize, tip.y - inDir.y * kHeadSize };
    ImVec2 const p1{ back.x + perp.x * kHeadSize * 0.5f, back.y + perp.y * kHeadSize * 0.5f };
    ImVec2 const p2{ back.x - perp.x * kHeadSize * 0.5f, back.y - perp.y * kHeadSize * 0.5f };
    inDrawList->AddTriangleFilled( tip, p1, p2, inColor );
}

// Whether inScreenPos falls on the arm from inAnchor extending kGizmoLength along inDir (a unit axis vector).
bool OnArm( asge::math::Float2 inScreenPos, ImVec2 inAnchor, ImVec2 inDir ) noexcept
{
    float const ex = inAnchor.x + inDir.x * kGizmoLength;
    float const ey = inAnchor.y + inDir.y * kGizmoLength;
    float const x0 = std::min( inAnchor.x, ex ) - kGizmoHitPadding;
    float const x1 = std::max( inAnchor.x, ex ) + kGizmoHitPadding;
    float const y0 = std::min( inAnchor.y, ey ) - kGizmoHitPadding;
    float const y1 = std::max( inAnchor.y, ey ) + kGizmoHitPadding;
    return inScreenPos.x() >= x0 && inScreenPos.x() <= x1 && inScreenPos.y() >= y0 && inScreenPos.y() <= y1;
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

void DrawTranslateGizmo(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inSelected, ImDrawList* inDrawList ) noexcept
{
    auto const handles = ComputeGizmoHandles( inRenderer, inRegistry, inSelected );
    if ( !handles ) return;

    DrawArrow( inDrawList, handles->east,  ImVec2{  1.0f,  0.0f }, kGizmoXColor );
    DrawArrow( inDrawList, handles->west,  ImVec2{ -1.0f,  0.0f }, kGizmoXColor );
    DrawArrow( inDrawList, handles->north, ImVec2{  0.0f, -1.0f }, kGizmoYColor );
    DrawArrow( inDrawList, handles->south, ImVec2{  0.0f,  1.0f }, kGizmoYColor );
}

GizmoAxis HitTestGizmo(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inSelected, asge::math::Float2 inScreenPos ) noexcept
{
    auto const handles = ComputeGizmoHandles( inRenderer, inRegistry, inSelected );
    if ( !handles ) return GizmoAxis::None;

    if ( OnArm( inScreenPos, handles->east,  ImVec2{  1.0f,  0.0f } )
      || OnArm( inScreenPos, handles->west,  ImVec2{ -1.0f,  0.0f } ) ) return GizmoAxis::X;

    if ( OnArm( inScreenPos, handles->north, ImVec2{  0.0f, -1.0f } )
      || OnArm( inScreenPos, handles->south, ImVec2{  0.0f,  1.0f } ) ) return GizmoAxis::Y;

    return GizmoAxis::None;
}

#include "ViewportOverlay.hpp"

#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Utils/SpriteGeometry.hpp>
#include <ASGE/Game/Components/Collider.hpp>
#include <ASGE/Game/Components/Camera.hpp>
#include <ASGE/Core/ECS/Hierarchy.hpp>
#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Core/Math/Interpolation/CatmullRomSpline.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <variant>

using namespace asge::game::components;
using asge::game::utils::SpriteGetDstRect;

asge::math::Rect GetEntityWorldBounds(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity, Transform const& inTransform ) noexcept
{
    asge::math::Rect bounds{ inTransform.m_WorldCoordinates.x(), inTransform.m_WorldCoordinates.y(), 1.0f, 1.0f };
    if ( auto spriteResult = inRegistry.GetComponent<Sprite>( inEntity ) )
    {
        if ( auto dst = SpriteGetDstRect( spriteResult.Value().get(), inTransform ) ) bounds = *dst;
    }
    else if ( auto rectResult = inRegistry.GetComponent<UIRect>( inEntity ) )
    {
        // Same RectFromSize math RenderSystem.cpp's own Collect<UIRect> uses
        // for its destination rect -- a UI widget (Button/Label) is picked
        // by its real footprint, not the generic point fallback below.
        auto const& size = rectResult.Value().get().m_Size;
        bounds = asge::math::Rect{
            inTransform.m_WorldCoordinates.x(), inTransform.m_WorldCoordinates.y(),
            size.x() * inTransform.m_WorldScale.x(), size.y() * inTransform.m_WorldScale.y() };
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
    if ( transformResult.Value().get().m_Locked ) return std::nullopt; // no arms on an entity that can't move

    auto const worldBounds = GetEntityWorldBounds( inRegistry, inSelected, transformResult.Value().get() );

    // The editor sets resources::ScreenSpaceCamera to this same live camera
    // every frame (see main.cpp), so RenderSystem draws a screen-space
    // entity through it too -- the gizmo can just use it uniformly for every
    // entity rather than re-deriving which camera a given entity resolves
    // under.
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
        if ( inRegistry.IsDisabled( entity ) ) continue;

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
            asge::math::Float2 const worldMin{ t.m_WorldCoordinates.x() + rect->m_X, t.m_WorldCoordinates.y() + rect->m_Y };
            asge::math::Float2 const worldMax{ worldMin.x() + rect->m_Width, worldMin.y() + rect->m_Height };
            auto const screenMin = asge::video::WorldToScreen( camera, viewport, worldMin );
            auto const screenMax = asge::video::WorldToScreen( camera, viewport, worldMax );

            inDrawList->AddRectFilled( ToImVec2( screenMin ), ToImVec2( screenMax ), fillColor );
            inDrawList->AddRect( ToImVec2( screenMin ), ToImVec2( screenMax ), kOutlineColor );
        }
        else if ( auto const* circle = std::get_if<asge::math::Circle>( &collider.m_LocalBounds ) )
        {
            asge::math::Float2 const worldCenter{ t.m_WorldCoordinates.x() + circle->m_Center.x(), t.m_WorldCoordinates.y() + circle->m_Center.y() };
            auto const screenCenter = asge::video::WorldToScreen( camera, viewport, worldCenter );
            float const screenRadius = circle->m_Radius * camera.m_Zoom;

            inDrawList->AddCircleFilled( ToImVec2( screenCenter ), screenRadius, fillColor );
            inDrawList->AddCircle( ToImVec2( screenCenter ), screenRadius, kOutlineColor );
        }
    }
}

void DrawGameWindowPreview(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList,
    int inTargetWidth, int inTargetHeight ) noexcept
{
    if ( inTargetWidth <= 0 || inTargetHeight <= 0 ) return;

    asge::math::Rect const worldRect{
        0.0f, 0.0f, static_cast<float>( inTargetWidth ), static_cast<float>( inTargetHeight ) };

    // Through the EDITOR's own camera/viewport (like every other overlay
    // here), not inRenderer.GetViewport() alone -- this is what makes the
    // preview actually pan/zoom with the editor instead of always just
    // fitting whatever the current window happens to be.
    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();
    auto const screenMin = asge::video::WorldToScreen( camera, viewport, { worldRect.m_X, worldRect.m_Y } );
    auto const screenMax = asge::video::WorldToScreen(
        camera, viewport, { worldRect.m_X + worldRect.m_Width, worldRect.m_Y + worldRect.m_Height } );

    float const vpLeft = viewport.m_X, vpTop = viewport.m_Y;
    float const vpRight = viewport.m_X + viewport.m_Width, vpBottom = viewport.m_Y + viewport.m_Height;

    // Dim everything in the current viewport outside the (clamped) preview
    // rect -- general masking, since the rect can sit anywhere (or nowhere:
    // fully off-screen clamps to a zero-width sliver, dimming the whole
    // viewport, which correctly signals "the game's view is out of frame").
    float const rectLeft   = std::clamp( screenMin.x(), vpLeft, vpRight );
    float const rectTop    = std::clamp( screenMin.y(), vpTop, vpBottom );
    float const rectRight  = std::clamp( screenMax.x(), vpLeft, vpRight );
    float const rectBottom = std::clamp( screenMax.y(), vpTop, vpBottom );

    constexpr ImU32 kBarColor = IM_COL32( 0, 0, 0, 140 );
    if ( rectTop > vpTop )
        inDrawList->AddRectFilled( ImVec2( vpLeft, vpTop ), ImVec2( vpRight, rectTop ), kBarColor );
    if ( rectBottom < vpBottom )
        inDrawList->AddRectFilled( ImVec2( vpLeft, rectBottom ), ImVec2( vpRight, vpBottom ), kBarColor );
    if ( rectLeft > vpLeft )
        inDrawList->AddRectFilled( ImVec2( vpLeft, rectTop ), ImVec2( rectLeft, rectBottom ), kBarColor );
    if ( rectRight < vpRight )
        inDrawList->AddRectFilled( ImVec2( rectRight, rectTop ), ImVec2( vpRight, rectBottom ), kBarColor );

    constexpr ImU32 kBorderColor = IM_COL32( 255, 210, 60, 220 );
    inDrawList->AddRect(
        ImVec2( screenMin.x(), screenMin.y() ), ImVec2( screenMax.x(), screenMax.y() ), kBorderColor, 0.0f, 0, 2.0f );

    // Otherwise unlabeled, this border is easy to mistake for a stray/buggy
    // rectangle rather than what it actually is. Anchored to the clamped
    // corner (not the true, possibly off-screen one) so it stays readable
    // even when the rect itself is mostly panned/zoomed out of view.
    char label[32];
    std::snprintf( label, sizeof( label ), "Target: %dx%d", inTargetWidth, inTargetHeight );
    inDrawList->AddText( ImVec2( rectLeft + 6.0f, rectTop + 6.0f ), kBorderColor, label );
}

void DrawCameraOverlays(
    asge::video::IRenderer const& inRenderer, asge::ecs::Registry& inRegistry, ImDrawList* inDrawList,
    int inTargetWidth, int inTargetHeight ) noexcept
{
    if ( inTargetWidth <= 0 || inTargetHeight <= 0 ) return;

    auto const& renderCamera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();

    constexpr ImU32 kFillColor = IM_COL32( 255, 210, 60, 40 );
    constexpr ImU32 kOutlineColor = IM_COL32( 255, 210, 60, 220 );

    // Same shape as DrawColliderOverlays: every entity carrying the
    // component gets its own box, purely a read-only preview -- unlike
    // systems::CameraSystem, this never calls IRenderer::SetCamera, so it
    // can never move the editor's own pan/zoom no matter how many Camera
    // entities exist or which (if any) is resources::ActiveCamera.
    for ( auto entity : inRegistry.AllEntities() )
    {
        if ( inRegistry.IsDisabled( entity ) ) continue;

        auto cameraResult = inRegistry.GetComponent<Camera>( entity );
        auto transformResult = inRegistry.GetComponent<Transform>( entity );
        if ( !cameraResult || !transformResult ) continue;

        // Mirrors systems::CameraSystem's own follow-point math exactly
        // (RenderSystem.cpp): a Sprite's own visual center (its dest rect's
        // midpoint) if this entity has one resolved, since Transform::
        // m_X/m_Y is that rect's top-left corner, not its middle -- else
        // the raw Transform point. Were this entity the active camera in a
        // inTargetWidth x inTargetHeight game window, it'd end up centered
        // in it at its own Camera::m_Zoom -- this box is exactly that.
        float const zoom = cameraResult.Value().get().m_Zoom;
        auto const& t = transformResult.Value().get();
        float followX = t.m_WorldCoordinates.x();
        float followY = t.m_WorldCoordinates.y();
        if ( auto spriteResult = inRegistry.GetComponent<Sprite>( entity ) )
        {
            if ( auto dst = SpriteGetDstRect( spriteResult.Value().get(), t ) )
            {
                followX = dst->m_X + dst->m_Width  * 0.5f;
                followY = dst->m_Y + dst->m_Height * 0.5f;
            }
        }

        asge::math::Rect const worldRect{
            followX - static_cast<float>( inTargetWidth )  / ( 2.0f * zoom ),
            followY - static_cast<float>( inTargetHeight ) / ( 2.0f * zoom ),
            static_cast<float>( inTargetWidth )  / zoom,
            static_cast<float>( inTargetHeight ) / zoom
        };

        auto const screenMin = asge::video::WorldToScreen(
            renderCamera, viewport, { worldRect.m_X, worldRect.m_Y } );
        auto const screenMax = asge::video::WorldToScreen(
            renderCamera, viewport, { worldRect.m_X + worldRect.m_Width, worldRect.m_Y + worldRect.m_Height } );

        inDrawList->AddRectFilled( ToImVec2( screenMin ), ToImVec2( screenMax ), kFillColor );
        inDrawList->AddRect( ToImVec2( screenMin ), ToImVec2( screenMax ), kOutlineColor, 0.0f, 0, 2.0f );

        // The box's center IS the follow point computed above -- marked
        // explicitly since a Camera-only entity (no Sprite) otherwise has
        // nothing else drawn to show where its own position actually is,
        // making "is this centered on it?" impossible to eyeball.
        auto const screenCenter = asge::video::WorldToScreen( renderCamera, viewport, { followX, followY } );
        inDrawList->AddCircleFilled( ToImVec2( screenCenter ), 4.0f, kOutlineColor );

        // worldRect.m_Width/m_Height (inTargetWidth/inTargetHeight scaled by
        // this entity's own zoom), not the raw inTargetWidth/inTargetHeight
        // -- two cameras at different zoom show visibly different-sized
        // boxes, so the label needs to actually match the box it's on
        // rather than printing the same window-pixel size for both.
        char label[48];
        std::snprintf( label, sizeof( label ), "Camera %.0fx%.0f (%.2gx zoom)",
            worldRect.m_Width, worldRect.m_Height, zoom );
        inDrawList->AddText( ImVec2( screenMin.x() + 6.0f, screenMin.y() + 6.0f ), kOutlineColor, label );
    }
}

asge::math::Float2 PathLocalToWorld( asge::game::components::Transform const& inRoute, asge::math::Float2 inLocal ) noexcept
{
    float const c = std::cos( inRoute.m_WorldRotation );
    float const s = std::sin( inRoute.m_WorldRotation );
    float const x = inLocal.x() * inRoute.m_WorldScale.x();
    float const y = inLocal.y() * inRoute.m_WorldScale.y();
    return { inRoute.m_WorldCoordinates.x() + x * c - y * s, inRoute.m_WorldCoordinates.y() + x * s + y * c };
}

asge::math::Float2 PathWorldToLocal( asge::game::components::Transform const& inRoute, asge::math::Float2 inWorld ) noexcept
{
    float const c = std::cos( inRoute.m_WorldRotation );
    float const s = std::sin( inRoute.m_WorldRotation );
    float const dx = inWorld.x() - inRoute.m_WorldCoordinates.x();
    float const dy = inWorld.y() - inRoute.m_WorldCoordinates.y();
    float const x = dx * c + dy * s;
    float const y = -dx * s + dy * c;
    float const sx = inRoute.m_WorldScale.x();
    float const sy = inRoute.m_WorldScale.y();
    return { sx != 0.0f ? x / sx : 0.0f, sy != 0.0f ? y / sy : 0.0f };
}

void DrawPathWaypointOverlay(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList,
    asge::game::components::Transform const& inRoute, asge::game::components::Path const& inPath,
    bool inActive, int inSelectedIndex ) noexcept
{
    auto const& waypoints = inPath.m_Waypoints;
    if ( waypoints.empty() ) return;

    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();
    auto const toScreen = [&]( asge::math::Float2 inLocal ) noexcept
    { return ToImVec2( asge::video::WorldToScreen( camera, viewport, PathLocalToWorld( inRoute, inLocal ) ) ); };

    ImU32 const kFillColor = inActive ? IM_COL32( 255, 220, 70, 230 ) : IM_COL32( 255, 210, 60, 130 );
    ImU32 const kLineColor = inActive ? IM_COL32( 255, 220, 70, 200 ) : IM_COL32( 255, 210, 60, 90 );
    float const r = inActive ? 9.0f : 6.0f; // fixed screen pixels -- not scaled by camera.m_Zoom, so markers stay the same size at any zoom

    for ( std::size_t i = 1; i < waypoints.size(); ++i )
    {
        inDrawList->AddLine( toScreen( waypoints[i - 1] ), toScreen( waypoints[i] ), kLineColor, 2.0f );
    }

    if ( !inActive && waypoints.size() >= 2 )
    {
        constexpr ImU32 kSplineColor = IM_COL32( 90, 190, 255, 200 );
        asge::math::CatmullRomSpline const spline( waypoints, inPath.m_Resolution );
        if ( spline.HasSegments() )
        {
            int const samples = std::max(
                2, static_cast<int>( spline.Segments().size() ) * static_cast<int>( inPath.m_Resolution ) );
            auto previous = toScreen( spline.PointAt( 0.0f ) );
            for ( int i = 1; i <= samples; ++i )
            {
                float const t = static_cast<float>( i ) / static_cast<float>( samples );
                auto const curr = toScreen( spline.PointAt( t ) );
                inDrawList->AddLine( previous, curr, kSplineColor, 2.5f );
                previous = curr;
            }
        }
    }

    for ( std::size_t i = 0; i < waypoints.size(); ++i )
    {
        auto const tip = toScreen( waypoints[i] );
        ImVec2 const head{ tip.x, tip.y - r * 1.4f };
        inDrawList->AddCircleFilled( head, r, kFillColor );
        ImVec2 const left{ head.x - r * 0.85f, head.y + r * 0.55f };
        ImVec2 const right{ head.x + r * 0.85f, head.y + r * 0.55f };
        inDrawList->AddTriangleFilled( left, right, tip, kFillColor );
        if ( static_cast<int>( i ) == inSelectedIndex ) inDrawList->AddCircle( head, r + 2.0f, IM_COL32( 255, 80, 80, 255 ), 0, 3.0f );
        if ( i == 0 ) inDrawList->AddCircle( head, r + 3.0f, IM_COL32( 255, 255, 255, 220 ), 0, 2.0f ); // locked to the route origin
    }
}

std::optional<PathPose> SamplePathPose(
    asge::game::components::Path const& inPath, asge::game::components::PathFollow const& inFollow,
    float inDistance ) noexcept
{
    asge::math::CatmullRomSpline built;
    asge::math::CatmullRomSpline const* spline = &inPath.m_Path;
    if ( !spline->HasSegments() )
    {
        built = asge::math::CatmullRomSpline( inPath.m_Waypoints, inPath.m_Resolution );
        spline = &built;
    }
    if ( !spline->HasSegments() ) return std::nullopt;

    PathPose pose;
    auto const point = spline->PointAtDistance( inDistance );
    pose.m_Position = { point.x() + inFollow.m_OffsetH, point.y() + inFollow.m_OffsetV };
    auto const tangent = spline->TangentAt( spline->TimeAtDistance( inDistance ) );
    if ( std::isfinite( tangent.x() ) && std::isfinite( tangent.y() ) )
    {
        pose.m_Heading = tangent;
        if ( inFollow.m_AlignRotation ) pose.m_Rotation = std::atan2( tangent.y(), tangent.x() );
    }
    return pose;
}

void DrawPathFollowerHeading(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inFollower ) noexcept
{
    using namespace asge::game::components;
    auto const follow = inRegistry.GetComponent<PathFollow>( inFollower );
    auto const hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( inFollower );
    if ( !follow || !hierarchy ) return;

    auto const routeEntity = hierarchy.Value().get().m_Parent;
    if ( routeEntity == asge::ecs::Entity::Null() ) return;
    auto const route = inRegistry.GetComponent<Transform>( routeEntity );
    auto const path = inRegistry.GetComponent<Path>( routeEntity );
    if ( !route || !path ) return;

    // Sampled at the follower's own progress, so it matches what PathFollowingSystem would write there.
    auto const pose = SamplePathPose( path.Value().get(), follow.Value().get(), follow.Value().get().m_Traveled );
    if ( !pose || !pose->m_Heading ) return;

    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();
    auto const toScreen = [&]( asge::math::Float2 inWorld ) noexcept
    { return ToImVec2( asge::video::WorldToScreen( camera, viewport, inWorld ) ); };

    // Two points through the route's transform, so its rotation and scale carry over to the direction.
    auto const& routeTransform = route.Value().get();
    ImVec2 const from = toScreen( PathLocalToWorld( routeTransform, pose->m_Position ) );
    ImVec2 const to = toScreen( PathLocalToWorld( routeTransform, pose->m_Position + *pose->m_Heading ) );
    float const dx = to.x - from.x;
    float const dy = to.y - from.y;
    float const len = std::sqrt( dx * dx + dy * dy );
    if ( len < 1e-4f ) return;

    constexpr float kArrowLength = 48.0f; // screen pixels, so it reads the same at any zoom
    constexpr float kHeadSize = 10.0f;
    ImU32 const color = IM_COL32( 255, 200, 60, 255 );
    ImVec2 const dir{ dx / len, dy / len };
    ImVec2 const tip{ from.x + dir.x * kArrowLength, from.y + dir.y * kArrowLength };
    ImVec2 const base{ tip.x - dir.x * kHeadSize, tip.y - dir.y * kHeadSize };
    ImVec2 const side{ -dir.y * kHeadSize * 0.5f, dir.x * kHeadSize * 0.5f };
    inDrawList->AddLine( from, base, color, 2.5f );
    inDrawList->AddTriangleFilled( tip, ImVec2( base.x + side.x, base.y + side.y ), ImVec2( base.x - side.x, base.y - side.y ), color );
    inDrawList->AddCircleFilled( from, 3.5f, color );
}

void DrawPathFollowerGhosts(
    asge::video::IRenderer const& inRenderer, ImDrawList* inDrawList, asge::ecs::Registry& inRegistry,
    asge::ecs::Entity inRoute, float inProgress ) noexcept
{
    using namespace asge::game::components;
    auto const route = inRegistry.GetComponent<Transform>( inRoute );
    auto const path = inRegistry.GetComponent<Path>( inRoute );
    if ( !route || !path ) return;

    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();
    auto const toScreen = [&]( asge::math::Float2 inWorld ) noexcept
    { return ToImVec2( asge::video::WorldToScreen( camera, viewport, inWorld ) ); };

    float const length = path.Value().get().m_Path.Length();

    std::vector<asge::ecs::Entity> followers;
    asge::ecs::components::ForEachChild( inRegistry, inRoute, [&]( asge::ecs::Entity inChild )
    { followers.push_back( inChild ); } );

    for ( auto const follower : followers )
    {
        auto const follow = inRegistry.GetComponent<PathFollow>( follower );
        auto const sprite = inRegistry.GetComponent<Sprite>( follower );
        auto const transform = inRegistry.GetComponent<Transform>( follower );
        if ( !follow || !sprite || !transform || !sprite.Value().get().m_Texture ) continue;

        auto const& spr = sprite.Value().get();
        auto const localRect = asge::game::utils::SpriteGetLocalRect( spr, transform.Value().get() );
        if ( !localRect ) continue;

        auto const texSize = spr.m_Texture->Size();
        auto const src = spr.m_SourceRect.value_or(
            asge::math::Rect{ 0.0f, 0.0f, static_cast<float>( texSize.x() ), static_cast<float>( texSize.y() ) } );
        ImVec2 const uv0{ src.m_X / static_cast<float>( texSize.x() ), src.m_Y / static_cast<float>( texSize.y() ) };
        ImVec2 const uv1{ ( src.m_X + src.m_Width ) / static_cast<float>( texSize.x() ),
                          ( src.m_Y + src.m_Height ) / static_cast<float>( texSize.y() ) };

        auto const drawGhost = [&]( float inDistance, ImU32 inTint )
        {
            auto const pose = SamplePathPose( path.Value().get(), follow.Value().get(), inDistance );
            if ( !pose ) return;
            auto const origin = PathLocalToWorld( route.Value().get(), pose->m_Position );
            float const rotation = route.Value().get().m_WorldRotation
                + pose->m_Rotation.value_or( transform.Value().get().m_LocalRotation );
            asge::math::Rect const dst{
                origin.x(), origin.y(),
                localRect->m_Width * route.Value().get().m_WorldScale.x(),
                localRect->m_Height * route.Value().get().m_WorldScale.y() };
            auto const corners = asge::game::components::SpriteGetDrawCorners( dst, rotation );
            asge::math::Float2 const br{
                corners.m_Right.x() + corners.m_Down.x() - corners.m_Origin.x(),
                corners.m_Right.y() + corners.m_Down.y() - corners.m_Origin.y() };
            inDrawList->AddImageQuad(
                spr.m_Texture->NativeHandle(), toScreen( corners.m_Origin ), toScreen( corners.m_Right ),
                toScreen( br ), toScreen( corners.m_Down ), uv0, ImVec2( uv1.x, uv0.y ), uv1,
                ImVec2( uv0.x, uv1.y ), inTint );
        };

        drawGhost( 0.0f, IM_COL32( 120, 255, 140, 150 ) );
        if ( inProgress > 0.0f ) drawGhost( std::clamp( inProgress, 0.0f, 1.0f ) * length, IM_COL32( 255, 255, 255, 150 ) );
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

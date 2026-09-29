#include "RenderSystem.hpp"

#include <vector>
#include <algorithm>
#include <cmath>
#include <variant>
#include <utility>
#include <tuple>

#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/Camera.hpp>
#include <ASGE/Game/Components/RenderInfo.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Resources/ActiveCamera.hpp>
#include <ASGE/Game/Resources/HitEntry.hpp>
#include <ASGE/Video/Graphics/Camera.hpp>
#include <ASGE/Core/Math/Geometry/Collision.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Core/Functools.hpp>

namespace
{

using namespace asge;
using namespace asge::game::components;

/** @brief RenderInfo resolved up an entity's Hierarchy chain: layer/y-sort/screen-space plus which entity "owns" the group for tie-breaking. */
struct RenderInfoResolved
{
    int           m_Layer;      // Resolved draw-order bucket
    bool          m_YSort;      // Resolved y-sort opt-in
    bool          m_ScreenSpace;// Resolved screen-space flag
    ecs::Entity   m_Owner;      // Entity whose sort key this item shares (self, unless inheriting)
    std::uint32_t m_Depth;      // Hops up the inheritance chain to m_Owner; 0 if this entity owns its own sort key
    int           m_LocalOrder; // This entity's own RenderInfo::m_LocalOrder, tie-break among entities sharing m_Owner
};

using Visual = std::variant<Sprite const*, UIRect const*>;

/** @brief One drawable entity's precomputed sort keys and destination rect for a single frame. */
struct DrawItem
{
    ecs::Entity                        m_Entity;    // Source entity; index is the tie-break of last resort
    game::components::Transform const* m_Transform;
    Visual                             m_Visual;
    RenderInfoResolved                 m_RenderInfo;
    float                              m_SortY;     // Bottom edge (position.y + drawn height), for y-sort
    math::Rect                         m_DstRect;
};

static constexpr RenderInfo kDefaultRenderInfo = RenderInfo{}; // Fallback for entities with no RenderInfo component of their own

/** @brief Maps a visual component type T to the tuple of other component types that, if also present on the same entity, mean T should not be collected/drawn for it. Defaults to none. */
template<typename T> struct should_collect_trait { using Excludes = std::tuple<>; };
/** @brief A UIRect is skipped in favor of a Sprite on the same entity -- lets a scene author swap one for the other without both drawing on top of each other. */
template<> struct should_collect_trait<UIRect> { using Excludes = std::tuple<Sprite>; };

/** @brief Checks inE against each type in should_collect_trait<T>::Excludes via GetComponent, folded with ||. */
template<typename T, std::size_t ...Is>
bool ShouldExcludeImpl(
    ecs::Registry const& inReg, ecs::Entity const& inE, std::index_sequence<Is...>) noexcept
{
    using Excludes = should_collect_trait<T>::Excludes;
    return ( ( static_cast<bool>( inReg.GetComponent<std::tuple_element_t<Is, Excludes>>(inE) ) ) || ... );
}

/** @brief True if inE has any of the component types should_collect_trait<T>::Excludes lists, meaning Collect<T> should skip it. */
template<typename T>
bool ShouldExclude( ecs::Registry const& inReg, ecs::Entity const& inE ) noexcept
{
    using Excludes = should_collect_trait<T>::Excludes;
    static constexpr std::size_t Size = std::tuple_size_v<Excludes>;
    if ( Size == 0 ) return false;
    return ShouldExcludeImpl<T>( inReg, inE, std::make_index_sequence<Size>{} );
}

/**
 * @brief Resolves inTarget's effective RenderInfoResolved, walking up its
 *        Hierarchy chain when RenderInfo::m_InheritSortFromParent is set.
 *
 * m_ScreenSpace always propagates from an inheriting ancestor even when
 * m_InheritSortFromParent is false; layer, y-sort and the sort owner only
 * propagate when it's true.
 */
RenderInfoResolved ResolveRenderInfo( ecs::Registry const& inReg, ecs::Entity inTarget )
{
    auto riResult = inReg.GetComponent<RenderInfo>( inTarget );
    RenderInfo const& targetRi = riResult ? riResult.Value().get() : kDefaultRenderInfo;

    RenderInfoResolved resolved{
        targetRi.m_Layer, targetRi.m_YSort, targetRi.m_ScreenSpace,
        inTarget, 0, targetRi.m_LocalOrder
    };

    auto hResult = inReg.GetComponent<asge::ecs::components::Hierarchy>( inTarget );
    if ( !hResult || hResult.Value().get().m_Parent == ecs::Entity::Null() ) return resolved;

    auto const parent = ResolveRenderInfo( inReg, hResult.Value().get().m_Parent );

    // Screen space always propagates, independent of sort inheritance
    resolved.m_ScreenSpace = resolved.m_ScreenSpace || parent.m_ScreenSpace;

    if ( !targetRi.m_InheritSortFromParent ) return resolved;

    resolved.m_Layer = parent.m_Layer;
    resolved.m_YSort = parent.m_YSort;
    resolved.m_Owner = parent.m_Owner;
    resolved.m_Depth = parent.m_Depth + 1;
    return resolved;
}

/**
 * @brief Orders DrawItems for RenderSystem: world-space before screen-space,
 *        then by resolved layer, then bottom-edge Y when either side opted
 *        into y-sort, then by sort owner, RenderInfo::m_LocalOrder,
 *        inheritance depth, and finally entity index.
 */
bool operator<(DrawItem const& a, DrawItem const& b) noexcept
{
    auto const& ra = a.m_RenderInfo;
    auto const& rb = b.m_RenderInfo;

    // For different screenspaces the one with True value must be rendered at the end
    // ScreenSpace always renders on top of everything
    if ( ra.m_ScreenSpace != rb.m_ScreenSpace ) return !ra.m_ScreenSpace;
    if ( ra.m_Layer != rb.m_Layer ) return ra.m_Layer < rb.m_Layer;
    if ( ( ra.m_YSort || rb.m_YSort ) && a.m_SortY != b.m_SortY ) return a.m_SortY < b.m_SortY;
    if ( ra.m_Owner.m_Index != rb.m_Owner.m_Index ) return ra.m_Owner.m_Index < rb.m_Owner.m_Index;
    if ( ra.m_LocalOrder != rb.m_LocalOrder ) return ra.m_LocalOrder < rb.m_LocalOrder;
    if ( ra.m_Depth != rb.m_Depth ) return ra.m_Depth < rb.m_Depth;
    if ( a.m_Entity.m_Index != b.m_Entity.m_Index ) return a.m_Entity.m_Index < b.m_Entity.m_Index;
    return a.m_Visual.index() < b.m_Visual.index();
}

// ---- Size : The only per-type part of collection ----------------------------------------------

/** @brief Builds a world-space rect from inT's position/scale and a size authored in Transform-local units (e.g. UIRect::m_Size). */
math::Rect RectFromSize( Transform const& inT, math::Float2 const& inSize ) noexcept
{
    return math::Rect{
        inT.m_WorldCoordinates.x(), inT.m_WorldCoordinates.y(),
        inSize.x() * inT.m_WorldScale.x(), inSize.y() * inT.m_WorldScale.y() };
}

/** @brief Sprite's destination rect, or nullopt if it has no texture (see SpriteGetDstRect). */
std::optional<math::Rect> ComputeDstRect( Sprite const& inS, Transform const& inT ) noexcept
{
    auto const r = SpriteGetDstRect( inS, inT );
    return r.has_value() ? std::optional<math::Rect>{ *r } : std::nullopt;
}

/** @brief UIRect's destination rect -- always present, unlike Sprite's (a widget has no missing-texture case). */
std::optional<math::Rect> ComputeDstRect( UIRect const& inR, Transform const& inT ) noexcept
{
    return RectFromSize( inT, inR.m_Size );
}

/** @brief Destination rect of whatever visual the entity has, for computing an owner's bottom edge. */
std::optional<math::Rect> GetAnyDstRect( ecs::Registry const& inReg, ecs::Entity inE, Transform const& inT ) noexcept
{
    if ( auto s = inReg.GetComponent<Sprite>( inE ) ) return ComputeDstRect( s.Value().get(), inT );
    if ( auto r = inReg.GetComponent<UIRect>( inE ) ) return ComputeDstRect( r.Value().get(), inT );
    return std::nullopt;
}

/** @brief inOwner's own bottom-edge Y (Transform + Sprite, if it has one), for an item inheriting inOwner's sort key 
 * -- or inFallback if inOwner has no Transform. */
float ComputeOwnerSortY( ecs::Registry const& inReg, ecs::Entity inOwner, float inFallback ) noexcept
{
    auto tResult = inReg.GetComponent<Transform>( inOwner );
    if ( !tResult ) return inFallback;

    auto const& transform = tResult.Value().get();
    auto const  dst = GetAnyDstRect( inReg, inOwner, transform );
    return transform.m_WorldCoordinates.y() + ( dst ? dst->m_Height : 0.0f );
}

// ---- Collection: identical for every visual type ------------------------------

/** @brief Builds a DrawItem from an entity's Transform + Sprite, or nullopt if the sprite has no texture. */
template<typename T>
void Collect( ecs::Registry& inReg, math::Rect const& inVisible, std::vector<DrawItem>& outItems ) noexcept
{
    for ( auto [ entity, transform, visual ] : inReg.View<Transform, T>() )
    {
        if ( ShouldExclude<T>( inReg, entity ) ) continue;

        auto const& t = transform.get();
        auto const  dst = ComputeDstRect( visual.get(), t );
        if ( !dst ) continue;

        RenderInfoResolved const render = ResolveRenderInfo( inReg, entity );
        if ( !render.m_ScreenSpace && !math::AabbOverlap( *dst, inVisible ) ) continue;

        float const ownSortY = t.m_WorldCoordinates.y() + dst->m_Height;
        float const sortY = ( render.m_Owner == entity )
            ? ownSortY
            : ComputeOwnerSortY( inReg, render.m_Owner, ownSortY );

        outItems.push_back( DrawItem{ 
            entity, 
            &t, 
            &visual.get(),
            render, 
            sortY, 
            *dst } );
    }
}

// ---- Drawing: one overload per visual type ------------------------------------

/** @brief Picks inColors' m_PressedColor/m_HoverColor/m_Color by inE's sibling Interactable, shared by UIButton and UICheckbox's box fill. */
graphics::RGBA_Color PickStateColor(
    ecs::Registry const& inReg, ecs::Entity inE, details::StateColors const& inColors)
{
    if ( auto interactable = inReg.GetComponent<Interactable>( inE ) )
    {
        auto const& i = interactable.Value().get();
        if ( i.m_Held && i.m_Hovered ) return inColors.m_PressedColor;
        if ( i.m_Hovered ) return inColors.m_HoverColor;
    }

    return inColors.m_Color;
}

/** @brief Draws a Sprite's texture into inItem.m_DstRect, routing through the affine overloads when the entity's Transform is rotated. */
void Draw(
    [[maybe_unused]] ecs::Registry const& inReg,
    video::IRenderer& inRenderer, DrawItem const& inItem, Sprite const& inSprite )
{
    video::ITexture* texture = inSprite.m_Texture;
    auto const& src = inSprite.m_SourceRect;

    if ( inItem.m_Transform->m_WorldRotation == 0.0f )
    {
        if ( src.has_value() ) inRenderer.DrawTexture( *texture, *src, inItem.m_DstRect );
        else inRenderer.DrawTexture( *texture, inItem.m_DstRect );
        return;
    }

    auto const corners = SpriteGetDrawCorners( inItem.m_DstRect, inItem.m_Transform->m_WorldRotation );
    if ( src.has_value() )
        inRenderer.DrawTextureAffine( *texture, *src, corners.m_Origin, corners.m_Right, corners.m_Down );
    else
        inRenderer.DrawTextureAffine( *texture, corners.m_Origin, corners.m_Right, corners.m_Down );
}

void Draw(
    ecs::Registry const& inReg, video::IRenderer& inRenderer, 
    DrawItem const& inItem, UIButton const& inButton)
{
    auto const color = PickStateColor( inReg, inItem.m_Entity, inButton.m_Colors );
    inRenderer.DrawRect( inItem.m_DstRect, color, true );
}

/**
 * @brief Draws a UILabel's text with its baked Font atlas.
 *
 * Positioned within inItem.m_DstRect by inLabel.m_Align/m_VerticalAlign
 * using Font::Measure's real pixel size, then offset down by GetAscent()
 * since DrawString's position is the baseline, not a bounding-box corner
 * -- for Top/Left that puts the glyphs flush against the rect's own edges.
 * str::Justify is the wrong tool for the horizontal case despite the
 * similar name: it pads by *character count* for fixed-width text layout,
 * not by the pixel width a proportional font actually draws at, so passing
 * it inItem.m_DstRect.m_Width (pixels) as a character count padded with
 * far more space glyphs than intended.
 */
void Draw(
    [[maybe_unused]] ecs::Registry const& inReg, video::IRenderer& inRenderer,
    DrawItem const& inItem, UILabel const& inLabel)
{
    if ( inLabel.m_Text.empty() || inLabel.m_Font == nullptr || inLabel.m_Texture == nullptr ) return;

    math::Float2 const textSize = inLabel.m_Font->Measure( inLabel.m_Text );

    float penX = inItem.m_DstRect.m_X;
    if ( inLabel.m_Align == str::TextAlign::Center || inLabel.m_Align == str::TextAlign::Right )
    {
        float const slack = inItem.m_DstRect.m_Width - textSize.x();
        penX += ( inLabel.m_Align == str::TextAlign::Center ) ? slack * 0.5f : slack;
    }

    float penY = inItem.m_DstRect.m_Y;
    if ( inLabel.m_VerticalAlign == VerticalAlign::Center || inLabel.m_VerticalAlign == VerticalAlign::Bottom )
    {
        float const slack = inItem.m_DstRect.m_Height - textSize.y();
        penY += ( inLabel.m_VerticalAlign == VerticalAlign::Center ) ? slack * 0.5f : slack;
    }
    penY += static_cast<float>( inLabel.m_Font->GetAscent() );

    inRenderer.DrawString( inLabel.m_Text, *inLabel.m_Font, *inLabel.m_Texture, { penX, penY }, inLabel.m_Color );
}

/**
 * @brief Draws a UICheckbox's box (PickStateColor'd from m_BoxColors, like
 *        UIButton) and, if checked, an inset filled square in m_CheckColor
 *        on top -- inset 15% of the box's width/height on each side.
 */
void Draw(
    ecs::Registry const& inReg, video::IRenderer& inRenderer,
    DrawItem const& inItem, UICheckbox const& inCheckbox)
{
    math::Rect const& box = inItem.m_DstRect;
    auto const color = PickStateColor( inReg, inItem.m_Entity, inCheckbox.m_BoxColors );
    inRenderer.DrawRect( box, color, true );

    if ( !inCheckbox.m_Checked ) return;

    // Check mark: inner square, inset 15% of the box on each side
    float const insetX = box.m_Width  * 0.15f;
    float const insetY = box.m_Height * 0.15f;
    math::Rect const checkBox{
        box.m_X + insetX, box.m_Y + insetY,
        box.m_Width  - 2.0f * insetX,
        box.m_Height - 2.0f * insetY
    };

    inRenderer.DrawRect( checkBox, inCheckbox.m_CheckColor, true );
}

void Draw(
    ecs::Registry const& inReg, video::IRenderer& inRenderer,
    DrawItem const& inItem, UISlider const& inSlider)
{
    math::Rect const& box = inItem.m_DstRect;
    float const insetY = box.m_Height * kTrackBarSize;
    math::Float2 const valueAt = GetThumbPosition( inSlider, box );

    float const filled_end = valueAt.x(),
                filled_w   = valueAt.x() - box.m_X,
                empty_w    = box.m_Width - ( valueAt.x() - box.m_X ),
                tracked_h  = box.m_Height - 2.0f * insetY;

    // First we need to draw the two rects one filled and the other do not filled
    math::Rect const filled{ box.m_X, box.m_Y + insetY, filled_w, tracked_h };
    math::Rect const empty{ filled_end, box.m_Y + insetY, empty_w, tracked_h };

    inRenderer.DrawRect( filled, inSlider.m_TrackColor, true );
    inRenderer.DrawRect( empty, inSlider.m_TrackColor, false );

    // Then we need to draw the circle with origin at thumb pos and r = h / 2
    float const radius = box.m_Height / 2.0f;
    auto const color = PickStateColor( inReg, inItem.m_Entity, inSlider.m_ThumbColor );
    math::Int2 center = math::Int2{ static_cast<int>(valueAt.x()), static_cast<int>(valueAt.y()) };
    inRenderer.DrawCircle( center, radius, color, true );
}

/**
 * @brief Draws a UIRect by dispatching to whichever of UIWidgets it also
 *        carries (UIButton/UICheckbox pick their fill color from a sibling
 *        Interactable's m_Held/m_Hovered; a missing Interactable just means
 *        never hovered/held); does nothing if it carries none of them.
 */
void Draw(
    ecs::Registry const& inReg, video::IRenderer& inRenderer,
    DrawItem const& inItem, [[maybe_unused]] UIRect const& inRect )
{
    using UIWidgets = std::tuple<UIButton, UILabel, UICheckbox, UISlider>;
    functools::ForEachTupleType<UIWidgets>( [&]<typename T> 
        {
            if ( auto r = inReg.GetComponent<T>( inItem.m_Entity ) )
                Draw( inReg, inRenderer, inItem, r.Value().get() );
        } 
    );
}

// ----------- Hit List collection ------------------------------------

/**
 * @brief Rebuilds resources::UIHitList (if set) from this frame's sorted
 *        drawItems -- a no-op if that resource isn't present.
 *
 * One HitEntry per entity carrying both UIRect (its footprint) and an
 * *enabled* Interactable (opting it into hit-testing at all) -- even one
 * drawn as a Sprite (see ShouldExclude), since a hit entry only needs the
 * rect, not whatever actually got drawn. Skips a second adjacent DrawItem
 * from the same entity (one entity can produce several -- e.g. Sprite +
 * UILabel -- which after sorting end up adjacent).
 */
void CollectHitList( ecs::Registry& inReg, std::vector<DrawItem> const& inDrawItems )
{
    if ( auto hitList = inReg.GetResource<asge::game::resources::UIHitList>() )
    {
        auto& entries = hitList.Value().get().m_Entries;
        entries.clear();

        for ( auto const& item : inDrawItems )
        {
            auto rect = inReg.GetComponent<UIRect>( item.m_Entity );
            if ( !rect ) continue;

            auto interactable = inReg.GetComponent<Interactable>( item.m_Entity );
            if ( !interactable || !interactable.Value().get().m_Enabled ) continue;

            if ( !entries.empty() && entries.back().m_Entity == item.m_Entity ) continue;

            entries.push_back( {
                item.m_Entity,
                RectFromSize(*item.m_Transform, rect.Value().get().m_Size),
                item.m_RenderInfo.m_ScreenSpace } );
        }
    }
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

    // The point the camera actually follows -- a Sprite's own visual
    // center (its dest rect's midpoint) if this entity has one resolved,
    // since Transform::m_X/m_Y is that rect's top-left corner (see
    // SpriteGetDstRect), not its middle; centering the viewport on the
    // corner would render the sprite offset down-right from screen-center
    // instead of actually centered. Falls back to the raw Transform point
    // for an entity with no Sprite (or an unresolved one) to follow instead.
    float followX = transform.m_WorldCoordinates.x();
    float followY = transform.m_WorldCoordinates.y();
    if ( auto spriteResult = inRegistry.GetComponent<components::Sprite>( entity ) )
    {
        if ( auto dst = components::SpriteGetDstRect( spriteResult.Value().get(), transform ) )
        {
            followX = dst->m_X + dst->m_Width  * 0.5f;
            followY = dst->m_Y + dst->m_Height * 0.5f;
        }
    }

    video::Camera camera = inRenderer.GetCamera();
    camera.m_Zoom = cameraComp.m_Zoom;

    float const targetX = followX - inRenderer.GetViewport().m_Width  / ( 2.0f * camera.m_Zoom );
    float const targetY = followY - inRenderer.GetViewport().m_Height / ( 2.0f * camera.m_Zoom );

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

    Collect<components::Sprite>( inRegistry, visible, drawItems );
    Collect<components::UIRect>( inRegistry, visible, drawItems );

    std::sort( drawItems.begin(), drawItems.end());

    CollectHitList( inRegistry, drawItems );

    video::Camera const worldCamera = inRenderer.GetCamera();
    bool inScreenSpace = false;

    for ( auto const& drawItem : drawItems )
    {
        // Screen-space items sort last, so this switch happens at most once per frame
        if ( drawItem.m_RenderInfo.m_ScreenSpace && !inScreenSpace )
        {
            video::Camera screenCamera = worldCamera;
            screenCamera.m_X    = 0.0f;
            screenCamera.m_Y    = 0.0f;
            screenCamera.m_Zoom = 1.0f;
            inRenderer.SetCamera( screenCamera );
            inScreenSpace = true;
        }

        std::visit(
            [&]( auto const *visual ) { Draw( inRegistry, inRenderer, drawItem, *visual ); },
            drawItem.m_Visual
        );
    }

    // Required: CameraSystem smooths from inRenderer.GetCamera() next frame,
    // so leaving the screen camera set would make it restart from (0, 0).
    if ( inScreenSpace ) inRenderer.SetCamera( worldCamera );
}

void asge::game::systems::RenderPipeline(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer, float inDeltaTime) noexcept
{
    AnimationSystem( inRegistry, inDeltaTime );
    CameraSystem( inRegistry, inRenderer, inDeltaTime );
    RenderSystem( inRegistry, inRenderer );
}

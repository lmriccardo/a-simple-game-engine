#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Game/Systems/TransformPropagationSystem.hpp>
#include <ASGE/Game/Systems/UISystem.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/PathFollow.hpp>
#include <ASGE/Game/Components/Collider.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Scene/SceneId.hpp>
#include <ASGE/Game/Resources/ScreenSpaceCamera.hpp>
#include <ASGE/Game/UI.hpp>
#include <ASGE/Audio/AudioDevice.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include "Inspector.hpp"
#include "ViewportOverlay.hpp"
#include "ConsolePanel.hpp"
#include "FileDialog.hpp"
#include "AssetBrowser.hpp"
#include "AssetInspector.hpp"
#include "VfsPanel.hpp"
#include "ProjectManager.hpp"

namespace
{
using asge::game::components::Transform;
using asge::game::components::PathFollow;
using asge::game::components::Collider;
using asge::game::components::Animation;
using asge::game::components::StopAnimation;
using asge::game::components::Sprite;
using asge::game::components::AudioSource;
using asge::game::components::RenderInfo;
using asge::game::components::AttachChild;
using asge::game::components::DetachChild;
using asge::ecs::components::DestroyEntityGraph;

constexpr SDL_DialogFileFilter kProjectFileFilters[]{ { "Project (*.asgeproject)", "asgeproject" } };
constexpr SDL_DialogFileFilter kSceneFileFilters[]{ { "Scene (*.asgescene)", "asgescene" } };

/** @brief Phase 16: which UI widget "Create UI Element"'s Type Selection modal picked. */
enum class UIElementType { Button, Label, Checkbox, Slider, Panel };

/** @brief Phase 17: default footprint for each UI widget type, seeding the Creation modal's Size field. */
asge::math::Float2 DefaultUISize( UIElementType inType ) noexcept
{
    switch ( inType )
    {
    case UIElementType::Checkbox: return asge::game::ui::consts::kCheckboxSize;
    case UIElementType::Slider:   return asge::game::ui::consts::kSliderSize;
    case UIElementType::Panel:    return asge::game::ui::consts::kPanelSize;
    case UIElementType::Button:
    case UIElementType::Label:    break;
    }
    return asge::game::ui::consts::kButtonSize;
}

/** @brief float[4] (0..1, ImGui::ColorEdit4's own range) -> RGBA_Color (0..255). */
asge::graphics::RGBA_Color ColorFromFloat4( float const inRGBA[4] ) noexcept
{
    auto const toU8 = []( float inV ) noexcept
    { return static_cast<std::uint8_t>( std::clamp( inV, 0.0f, 1.0f ) * 255.0f + 0.5f ); };
    return { toU8( inRGBA[0] ), toU8( inRGBA[1] ), toU8( inRGBA[2] ), toU8( inRGBA[3] ) };
}

/** @brief The read-side counterpart to ColorFromFloat4. */
void FloatFromColor( asge::graphics::RGBA_Color inColor, float outRGBA[4] ) noexcept
{
    outRGBA[0] = inColor.r / 255.0f;
    outRGBA[1] = inColor.g / 255.0f;
    outRGBA[2] = inColor.b / 255.0f;
    outRGBA[3] = inColor.a / 255.0f;
}

/**
 * @brief Finds the topmost entity (by RenderSystem's own resolved draw
 *        order -- see IsDrawnAbove) whose GetEntityWorldBounds contains
 *        inWorldPos, or Entity::Null() if none match.
 *
 * No multi-select yet. Ties in draw order (e.g. neither has a Transform+
 * Sprite/UIRect RenderSystem would ever actually draw) fall back to
 * IsDrawnAbove's own entity-index tiebreak, same as RenderSystem's sort.
 */
asge::ecs::Entity PickEntityAt( asge::ecs::Registry& inRegistry, asge::math::Float2 inWorldPos ) noexcept
{
    auto picked = asge::ecs::Entity::Null();
    for ( auto entity : inRegistry.AllEntities() )
    {
        auto transformResult = inRegistry.GetComponent<Transform>( entity );
        if ( !transformResult ) continue;

        auto const hitRect = GetEntityWorldBounds( inRegistry, entity, transformResult.Value().get() );
        bool const hit = inWorldPos.x() >= hitRect.m_X && inWorldPos.x() <= hitRect.m_X + hitRect.m_Width
                       && inWorldPos.y() >= hitRect.m_Y && inWorldPos.y() <= hitRect.m_Y + hitRect.m_Height;
        if ( !hit ) continue;

        if ( picked == asge::ecs::Entity::Null() || asge::game::systems::IsDrawnAbove( inRegistry, entity, picked ) )
        {
            picked = entity;
        }
    }
    return picked;
}

/**
 * @brief Index of inWaypoints' closest point within a fixed screen-space
 *        pick radius of inScreenPos, or -1 if none are close enough.
 *
 * Fixed-pixel radius (not a world-space one) so a waypoint stays equally
 * grabbable at any zoom level, same idea as the translate gizmo's own arm
 * hit-testing (ViewportOverlay.cpp's OnArm).
 */
int PickWaypointAt(
    asge::video::IRenderer const& inRenderer, std::vector<asge::math::Float2> const& inWaypoints,
    asge::math::Float2 inScreenPos ) noexcept
{
    constexpr float kPickRadius = 10.0f;
    auto const& camera = inRenderer.GetCamera();
    auto const& viewport = inRenderer.GetViewport();

    int closest = -1;
    float closestDistSq = kPickRadius * kPickRadius;
    for ( std::size_t i = 0; i < inWaypoints.size(); ++i )
    {
        auto const screen = asge::video::WorldToScreen( camera, viewport, inWaypoints[i] );
        float const dx = screen.x() - inScreenPos.x();
        float const dy = screen.y() - inScreenPos.y();
        float const distSq = dx * dx + dy * dy;
        if ( distSq <= closestDistSq )
        {
            closest = static_cast<int>( i );
            closestDistSq = distSq;
        }
    }
    return closest;
}

/**
 * @brief Copies every serializable component inSource has onto a freshly
 *        created entity, tagged with inScenePath's SceneId so it's included
 *        in ActiveEntities()/SaveScene() alongside everything else -- the
 *        same fold-over-SerializableComponents pattern SceneManager itself
 *        already uses for its own save-snapshot and cross-scene copies
 *        (SceneId isn't serializable data, so that generic copy can't
 *        carry it; this tags it separately as the one extra step needed).
 *
 * Hierarchy is the one component NOT copied: its links are structure, not
 * data, so a verbatim copy would claim the source's parent, siblings and
 * children without being in any of their lists. Instead each child of
 * inSource is duplicated (recursively) and attached under the new entity,
 * and the copy is left a root -- DuplicateEntity attaches it to a parent.
 */
asge::ecs::Entity DuplicateSubtree(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSource, asge::str::String const& inScenePath ) noexcept
{
    auto created = inRegistry.CreateEntity();
    if ( !created ) return asge::ecs::Entity::Null();
    auto const copy = created.Value();

    std::apply( [&]( auto ... component )
    {
        ( [&]
        {
            using T = decltype(component);
            if constexpr ( !std::is_same_v<T, asge::ecs::components::Hierarchy> )
            {
                if ( inRegistry.HasComponent<T>( inSource ) )
                {
                    inRegistry.AddComponent<T>( copy, inRegistry.GetComponent<T>( inSource ).Value().get() );
                }
            }
        }(), ... );
    }, asge::game::components::SerializableComponents{} );

    inRegistry.AddComponent<asge::game::scene::SceneId>( copy, asge::game::scene::SceneId{ inScenePath } );

    std::vector<asge::ecs::Entity> children;
    asge::ecs::components::ForEachChild( inRegistry, inSource, [&]( asge::ecs::Entity inChild )
    { children.push_back( inChild ); } );
    for ( auto child : children )
    {
        auto const childCopy = DuplicateSubtree( inRegistry, child, inScenePath );
        if ( childCopy != asge::ecs::Entity::Null() ) AttachChild( inRegistry, copy, childCopy );
    }

    return copy;
}

/**
 * @brief Duplicates inSource and everything under it (see DuplicateSubtree),
 *        and puts the copy in the same parent as inSource, after its existing
 *        children -- so a duplicate inside a UIPanel joins that panel's
 *        layout as its next slot instead of floating outside it.
 */
asge::ecs::Entity DuplicateEntity(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSource, asge::str::String const& inScenePath ) noexcept
{
    auto const copy = DuplicateSubtree( inRegistry, inSource, inScenePath );
    if ( copy == asge::ecs::Entity::Null() ) return copy;

    if ( auto hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( inSource );
         hierarchy && hierarchy.Value().get().m_Parent != asge::ecs::Entity::Null() )
    {
        AttachChild( inRegistry, hierarchy.Value().get().m_Parent, copy );
    }

    return copy;
}

/**
 * @brief Phase 14: attaches inEntity the component "corresponding" to an
 *        asset of inKind, pointed at inPath -- Sprite::m_VirtualPath for a
 *        Texture, Animation::m_ClipPath for an Animation clip,
 *        AudioSource::m_VirtualClipPath for Audio. Backs both the Assets
 *        panel's "Create Entity" and "Attach To" context-menu actions, which
 *        differ only in whether inEntity is freshly created.
 *
 * A Sprite also gets a RenderInfo if it doesn't already have one (same
 * auto-attach Inspector.cpp's own Sprite Add-Component does) -- every Sprite
 * is meant to have one, not just ones added through the Inspector.
 */
void AttachAssetComponent(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity, AssetPickKind inKind, std::string const& inPath ) noexcept
{
    switch ( inKind )
    {
    case AssetPickKind::Texture:
    {
        Sprite sprite{};
        sprite.m_VirtualPath = inPath;
        inRegistry.AddComponent<Sprite>( inEntity, sprite );
        (void)inRegistry.GetOrAddComponent<RenderInfo>( inEntity );
        break;
    }
    case AssetPickKind::Animation:
    {
        Animation animation{};
        animation.m_ClipPath = inPath;
        StopAnimation( animation ); // same "don't auto-play in the editor" reasoning as Inspector's Add Component
        inRegistry.AddComponent<Animation>( inEntity, animation );
        break;
    }
    case AssetPickKind::Audio:
    {
        AudioSource audioSource{};
        audioSource.m_VirtualClipPath = inPath;
        inRegistry.AddComponent<AudioSource>( inEntity, audioSource );
        break;
    }
    case AssetPickKind::Font:
        // A font path alone isn't a component to attach -- see AssetBrowser.hpp's
        // own doc comment for why the Fonts section offers no such menu item.
        break;
    case AssetPickKind::None:
        break;
    }
}

/**
 * @brief Sets inWindow's title-bar/taskbar icon from ASGE_EDITOR_ICON_PATH
 *        (assets/icon.png, rasterized from docsite/site/assets/img/logo.svg).
 *        SDL copies the pixel data on SDL_SetWindowIcon, so the source Image
 *        need not outlive the call.
 */
void SetWindowIcon( SDL_Window* inWindow ) noexcept
{
    auto imageResult = asge::media::Image::Load( ASGE_EDITOR_ICON_PATH );
    if ( !imageResult )
    {
        imageResult.LogError();
        return;
    }

    auto const& image = imageResult.Value();
    auto const dims = image.Dimensions();
    auto* surface = SDL_CreateSurfaceFrom(
        dims.x(), dims.y(), SDL_PIXELFORMAT_RGBA32,
        const_cast<std::uint8_t*>( image.Data() ), static_cast<int>( image.Stride() ) );
    if ( surface == nullptr )
    {
        LOG_ERROR( "Failed to build window icon surface: ", SDL_GetError() );
        return;
    }

    SDL_SetWindowIcon( inWindow, surface );
    SDL_DestroySurface( surface );
}

/** @brief "ASGE Editor - <project name>" when a project is active, else the plain default title. */
void UpdateWindowTitle( SDL_Window* inWindow, Project const* inProject ) noexcept
{
    std::string const title = inProject
        ? "ASGE Editor - " + inProject->m_FilePath.stem().string()
        : "ASGE Editor";
    SDL_SetWindowTitle( inWindow, title.c_str() );
}

/** @brief Marks inProject's currently active scene (if any) unsaved -- a no-op with no project or no active scene. */
void MarkActiveSceneDirty( std::optional<Project>& inProject ) noexcept
{
    if ( !inProject ) return;
    if ( inProject->m_ActiveSceneIndex < 0
      || inProject->m_ActiveSceneIndex >= static_cast<int>( inProject->m_Scenes.size() ) ) return;
    inProject->m_Scenes[inProject->m_ActiveSceneIndex].m_Dirty = true;
}

/**
 * @brief Makes inProject.m_Scenes[inNewIndex] the active scene: saves the
 *        currently active one first if it's dirty (so switching, or
 *        creating a new scene, never silently loses edits), then
 *        SceneManager::LoadSceneFromFile makes the new one active.
 *
 * SceneManager itself now suspends the outgoing scene into an in-memory
 * snapshot and restores the incoming one (from a snapshot if it has one,
 * disk otherwise) as part of that call (see issue #109) -- exactly one
 * scene's entities are ever live in its Registry at a time, so
 * RenderSystem/DrawEntityListPanel/viewport picking/etc. (which all just
 * enumerate that whole Registry, with no scoping of their own) never see a
 * second, stale scene bleed in. This editor code used to force that same
 * single-residency invariant itself via an explicit EvictCachedScene() on
 * every switch -- destroying the outgoing scene's entities outright and
 * forcing a full disk re-read next time, throwing away SceneManager's own
 * residency cache entirely. Not needed anymore: switching back to an
 * already-visited scene is a fast in-memory restore again, not a disk read.
 */
void SwitchToScene(
    Project& inOutProject, int inNewIndex,
    asge::game::scene::SceneManager& inSceneManager, asge::game::asset::AssetManager& inAssets,
    asge::video::IRenderer& inRenderer, asge::ecs::Entity& ioSelected ) noexcept
{
    if ( inOutProject.m_ActiveSceneIndex >= 0
      && inOutProject.m_ActiveSceneIndex < static_cast<int>( inOutProject.m_Scenes.size() ) )
    {
        auto& current = inOutProject.m_Scenes[inOutProject.m_ActiveSceneIndex];
        if ( current.m_Dirty )
        {
            auto const saveResult = inSceneManager.SaveScene( current.m_Path );
            if ( saveResult ) current.m_Dirty = false; else saveResult.LogError();
        }
    }

    auto const& target = inOutProject.m_Scenes[inNewIndex];
    auto const loadResult = inSceneManager.LoadSceneFromFile( target.m_Path );
    if ( !loadResult )
    {
        loadResult.LogError();
        return;
    }

    inOutProject.m_ActiveSceneIndex = inNewIndex;
    ioSelected = asge::ecs::Entity::Null();
    ResetEntityDisplayIds();
    inAssets.ResolveAssets( inSceneManager.GetRegistry(), inRenderer );
    RegisterSceneAssets( inSceneManager.GetRegistry() );

    // Serializer<Animation>::ToToml never writes m_Playing (see its own doc
    // comment -- a scene file describes what an entity's animation IS, not
    // where a previous run left it), so FromToml always leaves every loaded
    // Animation at the struct's in-code default, m_Playing=true. Correct for
    // a real game loading a level; wrong here, where RenderPipeline runs
    // AnimationSystem every frame purely to render the viewport -- every
    // animated entity would otherwise start cycling the instant a scene
    // loads/switches, the same unwanted auto-play this editor's "Add
    // Component" already guards against for a freshly-added one.
    for ( auto [ entity, animation ] : inSceneManager.GetRegistry().View<Animation>() )
    {
        StopAnimation( animation.get() );
    }

    // Phase 14: a scene saved before RenderInfo existed (or a Sprite that
    // otherwise ended up on an entity without going through this editor's
    // own Add Component/Create Entity/Attach To, which already attach one --
    // see AttachAssetComponent) can have a Sprite with no RenderInfo. Give
    // it the same default those paths do, and persist it back to this
    // scene's own file immediately -- a one-time fixup per file, not
    // something that should need a second silent "it's dirty now" save
    // later to actually land on disk.
    bool migratedRenderInfo = false;
    for ( auto [entity, sprite] : inSceneManager.GetRegistry().View<Sprite>() )
    {
        (void)sprite;
        if ( inSceneManager.GetRegistry().HasComponent<RenderInfo>( entity ) ) continue;
        (void)inSceneManager.GetRegistry().GetOrAddComponent<RenderInfo>( entity );
        migratedRenderInfo = true;
    }
    if ( migratedRenderInfo )
    {
        auto const saveResult = inSceneManager.SaveScene( target.m_Path );
        if ( !saveResult ) saveResult.LogError();
    }
}

/**
 * @brief Adds a brand-new, empty scene named inName to inOutProject at
 *        <inOutProject.m_FilePath's own folder>/<inName>.asgescene, saves
 *        the currently active scene first if it's dirty (this is itself a
 *        scene switch), and makes the new one active.
 *
 * UnloadScene -> RenameActiveScene -> SaveScene is SceneManager's own
 * documented pattern for giving a brand-new, never-saved scene an identity
 * and an empty file on disk in one step, no new engine code needed. Shared
 * by the Create a Scene modal and Create a Project's own auto-created
 * "Empty Scene" -- every fresh project starts with one scene, not none.
 * @return False (logged) if the save failed; inOutProject is still updated
 *         either way, since the scene is real either way.
 */
bool CreateSceneInProject(
    Project& inOutProject, std::string const& inName,
    asge::game::scene::SceneManager& inSceneManager ) noexcept
{
    if ( inOutProject.m_ActiveSceneIndex >= 0
      && inOutProject.m_ActiveSceneIndex < static_cast<int>( inOutProject.m_Scenes.size() ) )
    {
        auto& active = inOutProject.m_Scenes[inOutProject.m_ActiveSceneIndex];
        if ( active.m_Dirty )
        {
            auto const saveResult = inSceneManager.SaveScene( active.m_Path );
            if ( saveResult ) active.m_Dirty = false; else saveResult.LogError();
        }
    }

    auto const newPath = inOutProject.m_FilePath.parent_path() / ( inName + ".asgescene" );
    inOutProject.m_Scenes.push_back( ProjectScene{ inName, newPath, false } );
    inOutProject.m_ActiveSceneIndex = static_cast<int>( inOutProject.m_Scenes.size() ) - 1;

    inSceneManager.UnloadScene();
    inSceneManager.RenameActiveScene( newPath.string() );
    auto const saveResult = inSceneManager.SaveScene( newPath );
    if ( !saveResult )
    {
        saveResult.LogError();
        return false;
    }
    LOG_INFO( "Scene created at ", newPath.string() );
    return true;
}
}

// Phase 2 proved select -> edit -> save -> reload round-trips through the
// engine's real Transform serialization. Phase 3 generalizes the inspector
// (see Inspector.hpp/.cpp) to every currently-serializable component type,
// plus an entity list panel driving the same selection state as viewport
// picking. Still no Game/IGameState dependency -- picking uses raw SDL
// mouse events rather than InputState/InputSystem, which exist to feed
// IGameState::Update()'s polling model the editor doesn't have.
int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::video::VideoSystem videoSys;
    // Resizable, unlike every other VideoSystem consumer (see Initialize's
    // own doc comment) -- a level can be bigger than any fixed editor window,
    // so the window itself shouldn't be a hard limit. The renderer's
    // viewport is kept in sync with the live window size once per frame
    // (see below) rather than off SDL_EVENT_WINDOW_RESIZED's own data1/
    // data2 -- during a live drag-resize, or right after a maximize,
    // SDL_Renderer's internal output size doesn't necessarily match that
    // event's payload yet, so a viewport set from it could be applied
    // before the renderer itself has actually caught up. Re-deriving from
    // IWindow::Size() every frame instead means the viewport is always
    // whatever's live at draw time.
    auto const initResult = videoSys.Initialize(
        "ASGE Editor", 1280, 720, asge::video::GraphicsBackend::SDL, /*inResizable=*/true);
    if (!initResult)
    {
        initResult.LogError();
        return 1;
    }

    asge::filesystem::VirtualFileSystem vfs;
    asge::game::asset::AssetManager assets(vfs);
    asge::game::scene::SceneManager sceneManager(vfs);

    // Only used for the Asset Inspector's audio preview (Phase 12) -- not a
    // dependency on Game/IGameState, same as everything else here. A failed
    // init just leaves the preview's Play/Pause/Rewind buttons non-functional
    // rather than aborting the editor, unlike VideoSystem's init above.
    asge::audio::AudioDevice audioDevice;
    if ( auto const audioInit = audioDevice.Initialize(); !audioInit ) audioInit.LogError();

    // No project is auto-loaded on startup beyond whatever asge.session
    // resumes (see below, right before the main loop) -- the editor
    // otherwise opens with nothing active, no mount, no scene, until the
    // user creates or opens a project.
    namespace fs = std::filesystem;

    auto* window   = static_cast<SDL_Window*>(videoSys.GetWindow().NativeHandle());
    auto* renderer = static_cast<SDL_Renderer*>(videoSys.GetRenderer().NativeHandle());
    SetWindowIcon(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // No imgui.ini -- ImGui otherwise persists every window's position/
    // size/collapsed-state there and reloads it next launch, which silently
    // overrides every ImGuiCond_FirstUseEver hint below after the first run
    // (ImGui treats a window with saved settings as "already placed").
    // Every panel should open at its coded default position every time.
    ImGui::GetIO().IniFilename = nullptr;
    // Draw the cursor ourselves rather than relying on SDL/X11's system
    // cursor -- under WSLg, the nested compositor's cursor-theme lookup can
    // silently fail, leaving the real OS cursor invisible (but still
    // functional) with no fix on the engine/editor side. This bypasses that
    // entirely instead of chasing an environment-specific rendering bug.
    ImGui::GetIO().MouseDrawCursor = true;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    InitConsolePanel(); // connects to Logger's OnLog signal -- there's no terminal to read LOG_* output from otherwise

    auto selectedEntity = asge::ecs::Entity::Null();
    AssetPick selectedAsset; // last entry clicked in the Assets panel, kept across frames for the Asset Inspector to show
    float gridSpacing = 50.0f; // world units between grid lines; editor-configurable, see the View menu's Grid modal

    // Phase 12: PathFollow's "Select Waypoints" viewport mode -- see
    // WaypointEditState's own doc comment. Self-healing rather than reset at
    // every place its target entity could become invalid (deleted, scene
    // switched/evicted): checked once per frame below, right before it's
    // used for anything.
    WaypointEditState waypointEdit;

    // The target game window size DrawCameraOverlays/DrawGameWindowPreview
    // size their previews against -- an editor display setting (saved/
    // restored via a Project's own [View] table, not as scene data) since
    // nothing in the engine's own scene/config schema currently models "the
    // target game's window size" for this to read instead.
    int targetGameWidth = 1280;
    int targetGameHeight = 720;

    // Draft copies the View menu's Grid/Game Window modals edit -- only
    // copied back into gridSpacing/targetGameWidth/targetGameHeight on
    // "Apply"; "Close" just drops the draft, discarding whatever was typed.
    // Seeded from the live values the frame each modal opens (not every
    // frame, which would stomp mid-edit drags back to the live value).
    float draftGridSpacing = gridSpacing;
    int draftTargetWidth = targetGameWidth;
    int draftTargetHeight = targetGameHeight;

    // Phase 11: Create a Project modal's own draft fields, seeded (name/
    // folder cleared, grid/window defaulted to the live values) the frame
    // the modal opens.
    char createProjectNameBuf[128] = "";
    fs::path createProjectFolder;
    float createProjectGrid = gridSpacing;
    int createProjectWidth = targetGameWidth;
    int createProjectHeight = targetGameHeight;

    // Create a Scene modal's own draft field, plus its placeholder text --
    // ImGui::InputTextWithHint needs the hint string alive every frame it's
    // drawn, not just the frame the modal opens, so this can't be a local
    // recomputed only on open the way the char buffer's reset is.
    char createSceneNameBuf[128] = "";
    std::string createScenePlaceholder;

    // Phase 16: Create UI Element modal's own draft fields -- reset by
    // resetUICreateDraft (below, near where the modals are drawn) each time
    // Type Selection picks a type, same "seeded fresh on open" convention as
    // the drafts above.
    UIElementType uiCreateType = UIElementType::Button;
    char uiCreateNameBuf[128] = "";
    float uiCreatePos[2]{ 0.0f, 0.0f };
    float uiCreateSize[2]{ asge::game::ui::consts::kButtonSize.x(), asge::game::ui::consts::kButtonSize.y() };
    bool uiCreateAutoSize = true;      // Label only -- nullopt m_Size (fit the text) vs. uiCreateSize
    bool uiCreateScreenSpace = true;
    bool uiCreateEnabled = true;       // Button only
    char uiCreateTextBuf[256] = "";
    std::string uiCreateFontPath; // Phase 16: picked from KnownFontPaths, not hand-typed -- see DrawAssetPathCombo
    int uiCreateFontPixelHeight = asge::game::ui::consts::kFontPixelHeight;
    int uiCreateAlign = static_cast<int>( asge::str::TextAlign::Left );
    int uiCreateVAlign = static_cast<int>( asge::game::components::VerticalAlign::Center );
    float uiCreateTextColor[4]{};
    float uiCreateButtonColor[4]{};
    float uiCreateButtonHoverColor[4]{};
    float uiCreateButtonPressedColor[4]{};
    // Phase 17: Checkbox/Slider/Panel drafts. The three Button colors above
    // double as a Checkbox's box colors and a Slider's thumb colors, since
    // all three are the same StateColors triple.
    bool uiCreateChecked = false;               // Checkbox only
    float uiCreateCheckColor[4]{};              // Checkbox only
    float uiCreateMin = 0.0f, uiCreateMax = 1.0f, uiCreateValue = 0.0f; // Slider only
    float uiCreateTrackColor[4]{};              // Slider only
    float uiCreateBackground[4]{};              // Panel only, from here down
    bool uiCreateBorder = true;
    float uiCreateBorderColor[4]{};
    float uiCreateMargin[2]{}, uiCreatePadding[2]{}, uiCreateSpacing[2]{};
    int uiCreateLayout = 0;                     // asge::game::components::PanelLayout
    int uiCreateLayoutRows = 3, uiCreateLayoutCols = 3;
    // Phase 17: "Panel Parent" section -- Null means no parent.
    asge::ecs::Entity uiCreateParent = asge::ecs::Entity::Null();

    // Scene panel's own rename field -- resynced from the active scene's
    // current name only when the active scene itself changes (tracked via
    // sceneNameBufSyncedIndex, -2 meaning "never synced"; -1 is a real,
    // valid "no active scene" state), not every frame, which would fight
    // in-progress typing.
    char sceneNameBuf[128] = "";
    int sceneNameBufSyncedIndex = -2;

    // The three (now four, see below) top-center HUD panels' combined
    // width, from last frame -- ImGui can't report a window's AutoResize
    // size before it's actually drawn once, so centering the group this
    // frame off last frame's total (updated after the last one is drawn
    // below) is the standard immediate-mode fix for that chicken-and-egg
    // problem. One frame of lag on a width change (e.g. the mouse-position
    // text getting longer) is imperceptible; 0 here just means frame one
    // starts slightly off-center and self-corrects on frame two.
    float hudGroupWidth = 0.0f;

    // Tracks whether DisplaySize changed since last frame, for the Scene
    // panel's own right-edge anchoring below -- re-snap to the edge only on
    // an actual resize, ImGuiCond_FirstUseEver otherwise, so the panel
    // stays freely user-draggable the rest of the time (forcing
    // ImGuiCond_Always unconditionally re-fights the user's own drag every
    // single frame, making the panel effectively immovable).
    ImVec2 lastDisplaySize{ 0.0f, 0.0f };

    // Phase 11: what used to be Save Session/Open Session is now a Project
    // (.asgeproject, explicitly saved) plus this -- nullopt until Create a
    // Project/Open... actually establishes one. Which of its scenes (if
    // any) is active is Project::m_ActiveSceneIndex, editor-only state, not
    // itself part of the .asgeproject file (see asge.session instead).
    std::optional<Project> currentProject;

    FileDialogResult createProjectFolderDialogResult;
    FileDialogResult saveProjectAsDialogResult;
    FileDialogResult openProjectDialogResult;
    FileDialogResult openSceneDialogResult;

    // asge.session -- the ambient "what does the editor currently look
    // like" state (active project + active scene), distinct from a Project
    // itself and auto-saved/loaded rather than explicitly, in the per-user
    // preferences directory (SDL_GetPrefPath: %APPDATA%\ASGE\Editor on
    // Windows, created on demand) rather than next to the executable, which
    // an installed editor (Program Files) can't write to, or the CWD.
    fs::path const sessionFilePath = [] {
        char* const pref = SDL_GetPrefPath( "ASGE", "Editor" );
        if ( !pref ) return fs::path( "asge.session" );
        fs::path path = fs::path( pref ) / "asge.session";
        SDL_free( pref );
        return path;
    }();
    float sessionAutosaveTimer = 0.0f;
    constexpr float kSessionAutosaveInterval = 30.0f;

    auto const saveEditorSessionNow = [&]() noexcept
    {
        std::optional<fs::path> const activeScenePath =
            ( currentProject && currentProject->m_ActiveSceneIndex >= 0
              && currentProject->m_ActiveSceneIndex < static_cast<int>( currentProject->m_Scenes.size() ) )
            ? std::optional<fs::path>( currentProject->m_Scenes[currentProject->m_ActiveSceneIndex].m_Path )
            : std::nullopt;
        std::optional<fs::path> const projectPath =
            currentProject ? std::optional<fs::path>( currentProject->m_FilePath ) : std::nullopt;
        if ( auto const r = SaveEditorSession( projectPath, activeScenePath, sessionFilePath ); !r ) r.LogError();
    };

    // Phase 4: viewport free-drag + translate gizmo. draggingEntity is the
    // entity currently being moved (Null() when nothing is); dragAxis is
    // GizmoAxis::None for unconstrained free-drag, or X/Y when the drag
    // started on a gizmo arm instead.
    auto draggingEntity = asge::ecs::Entity::Null();
    GizmoAxis dragAxis = GizmoAxis::None;

    // Phase 9: middle-mouse-drag camera pan -- independent of draggingEntity/
    // dragAxis above (a different mouse button), so both can never be
    // active from the same drag.
    bool panningCamera = false;

    // Phase 12: dragging an existing waypoint while "Select Waypoints" mode
    // is active -- a click that lands on an existing point moves it instead
    // of appending a new one. -1 means nothing's being dragged.
    int draggingWaypointIndex = -1;

    // Phase 12 step 4: Collider's "Draw Collider" viewport mode -- see
    // ColliderDrawState's own doc comment. colliderDragging/
    // colliderDragStartWorld are the drag's own ephemeral state, kept out
    // of ColliderDrawState the same way draggingWaypointIndex is kept out
    // of WaypointEditState.
    ColliderDrawState colliderDraw;
    bool colliderDragging = false;
    asge::math::Float2 colliderDragStartWorld{};

    // Resume whatever asge.session last remembered, if it exists and its
    // project still does too -- everything it needs (window, sceneManager,
    // assets, vfs, gridSpacing/targetGameWidth/targetGameHeight,
    // currentProject, selectedEntity) is already set up above.
    if ( fs::exists( sessionFilePath ) )
    {
        std::optional<fs::path> sessionProjectPath;
        std::optional<fs::path> sessionActiveScenePath;
        auto const sessionLoadResult = LoadEditorSession( sessionFilePath, sessionProjectPath, sessionActiveScenePath );
        if ( !sessionLoadResult ) sessionLoadResult.LogError();
        else if ( sessionProjectPath && fs::exists( *sessionProjectPath ) )
        {
            Project loaded;
            auto const projectResult = LoadProject(
                vfs, *sessionProjectPath, loaded, gridSpacing, targetGameWidth, targetGameHeight );
            if ( !projectResult )
            {
                projectResult.LogError();
            }
            else
            {
                currentProject = std::move( loaded );
                UpdateWindowTitle( window, &*currentProject );

                int resumeIndex = -1;
                if ( sessionActiveScenePath )
                {
                    for ( std::size_t i = 0; i < currentProject->m_Scenes.size(); ++i )
                    {
                        if ( currentProject->m_Scenes[i].m_Path == *sessionActiveScenePath )
                        {
                            resumeIndex = static_cast<int>( i );
                            break;
                        }
                    }
                }
                if ( resumeIndex < 0 && !currentProject->m_Scenes.empty() ) resumeIndex = 0;
                if ( resumeIndex >= 0 )
                {
                    SwitchToScene(
                        *currentProject, resumeIndex, sceneManager, assets, videoSys.GetRenderer(), selectedEntity );
                }
                LOG_INFO( "Resumed project from ", sessionProjectPath->string() );
            }
        }
    }

    // Phase 12: shared by File > Save/Save Scene and the CTRL+S/CTRL+P
    // shortcuts below, so there's one place that knows how to save each --
    // re-checks currentProject itself (not a possibly-stale hasProject/
    // hasActiveScene snapshot) since these can now fire from inside the
    // event loop, before this frame's menu-bar code computes those.
    auto const saveProjectNow = [&]() noexcept
    {
        if (!currentProject) return;
        auto const saveResult = SaveProject(
            vfs, sceneManager.GetRegistry(), *currentProject, gridSpacing, targetGameWidth, targetGameHeight);
        if (!saveResult) saveResult.LogError();
        else LOG_INFO("Project saved to ", currentProject->m_FilePath.string());
    };
    auto const saveActiveSceneNow = [&]() noexcept
    {
        if (!currentProject
         || currentProject->m_ActiveSceneIndex < 0
         || currentProject->m_ActiveSceneIndex >= static_cast<int>(currentProject->m_Scenes.size())) return;
        auto& active = currentProject->m_Scenes[currentProject->m_ActiveSceneIndex];
        auto const saveResult = sceneManager.SaveScene(active.m_Path);
        if (!saveResult) saveResult.LogError();
        else
        {
            active.m_Dirty = false;
            LOG_INFO("Scene saved to ", active.m_Path.string());
        }
    };

    // Phase 12 step 4: an entity's world-space origin, for converting the
    // "Draw Collider" viewport drag's absolute world points into
    // Collider::m_LocalBounds' Transform-relative offset (see
    // ViewportOverlay.cpp's DrawColliderOverlays for the same convention).
    // {0,0} if the entity has no Transform, same fallback GetEntityWorldBounds uses.
    auto const entityOrigin = [&]( asge::ecs::Entity inEntity ) noexcept -> asge::math::Float2
    {
        if ( auto t = sceneManager.GetRegistry().GetComponent<Transform>( inEntity ) )
        {
            return { t.Value().get().m_WorldCoordinates.x(), t.Value().get().m_WorldCoordinates.y() };
        }
        return {};
    };

    bool running = true;
    while (running)
    {
        // Self-heals WaypointEditState rather than resetting it at every
        // place its target entity could become invalid (Delete, a scene
        // switch/evict) -- one guard here covers all of them.
        if (waypointEdit.m_Active
         && !sceneManager.GetRegistry().HasComponent<PathFollow>(waypointEdit.m_Entity))
        {
            waypointEdit = WaypointEditState{};
            draggingWaypointIndex = -1;
        }
        if (colliderDraw.m_Active
         && !sceneManager.GetRegistry().HasComponent<Collider>(colliderDraw.m_Entity))
        {
            colliderDraw = ColliderDrawState{};
            colliderDragging = false;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN
                   && event.key.key == SDLK_ESCAPE
                   && waypointEdit.m_Active)
            {
                // Cancel -- restore whatever the entity's waypoints were
                // before this mode started.
                if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(waypointEdit.m_Entity))
                {
                    pf.Value().get().m_Waypoints = waypointEdit.m_Snapshot;
                    RebuildPath(pf.Value().get());
                }
                waypointEdit = WaypointEditState{};
                draggingWaypointIndex = -1;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN
                   && event.key.key == SDLK_ESCAPE
                   && colliderDraw.m_Active)
            {
                // Cancel -- restore whatever shape the entity had before
                // this mode started.
                if (auto collider = sceneManager.GetRegistry().GetComponent<Collider>(colliderDraw.m_Entity))
                {
                    collider.Value().get().m_LocalBounds = colliderDraw.m_Snapshot;
                }
                colliderDraw = ColliderDrawState{};
                colliderDragging = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN
                   && (event.key.mod & SDL_KMOD_CTRL)
                   && event.key.key == SDLK_S)
            {
                saveActiveSceneNow();
            }
            else if (event.type == SDL_EVENT_KEY_DOWN
                   && (event.key.mod & SDL_KMOD_CTRL)
                   && event.key.key == SDLK_P)
            {
                saveProjectNow();
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL && !ImGui::GetIO().WantCaptureMouse)
            {
                auto& videoRenderer = videoSys.GetRenderer();
                auto camera = videoRenderer.GetCamera();
                asge::math::Float2 const screenPos{ event.wheel.mouse_x, event.wheel.mouse_y };

                // World point under the cursor before zooming, so it's still
                // under the cursor after -- anchoring on the viewport's
                // origin instead (i.e. just changing m_Zoom) would make
                // scrolling near an edge fight the pan above rather than
                // complementing it.
                auto const worldBefore = asge::video::ScreenToWorld( camera, videoRenderer.GetViewport(), screenPos );

                constexpr float kZoomStep = 1.1f;
                camera.m_Zoom *= event.wheel.y > 0.0f ? kZoomStep : 1.0f / kZoomStep;
                camera.m_Zoom = std::clamp( camera.m_Zoom, 0.1f, 10.0f );

                asge::math::Float2 const local{
                    screenPos.x() - videoRenderer.GetViewport().m_X, screenPos.y() - videoRenderer.GetViewport().m_Y };
                camera.m_X = worldBefore.x() - local.x() / camera.m_Zoom;
                camera.m_Y = worldBefore.y() - local.y() / camera.m_Zoom;

                videoRenderer.SetCamera( camera );
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                   && event.button.button == SDL_BUTTON_MIDDLE
                   && !ImGui::GetIO().WantCaptureMouse)
            {
                panningCamera = true;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_MIDDLE)
            {
                panningCamera = false;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                   && event.button.button == SDL_BUTTON_RIGHT
                   && !ImGui::GetIO().WantCaptureMouse
                   && waypointEdit.m_Active)
            {
                // Right-click removes the waypoint under the cursor, if any.
                asge::math::Float2 const screenPos{ event.button.x, event.button.y };
                if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(waypointEdit.m_Entity))
                {
                    auto& waypoints = pf.Value().get().m_Waypoints;
                    int const hitIndex = PickWaypointAt(videoSys.GetRenderer(), waypoints, screenPos);
                    if (hitIndex >= 0)
                    {
                        waypoints.erase(waypoints.begin() + hitIndex);
                        if (draggingWaypointIndex == hitIndex) draggingWaypointIndex = -1;
                        else if (draggingWaypointIndex > hitIndex) --draggingWaypointIndex;
                        RebuildPath(pf.Value().get());
                        MarkActiveSceneDirty(currentProject);
                    }
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                   && event.button.button == SDL_BUTTON_LEFT
                   && !ImGui::GetIO().WantCaptureMouse)
            {
                asge::math::Float2 const screenPos{ event.button.x, event.button.y };

                if (waypointEdit.m_Active)
                {
                    // Phase 12: a click in this mode either grabs an
                    // existing waypoint for dragging (landing on one) or
                    // appends a new one -- the target stays fixed for the
                    // whole mode, so this never touches selectedEntity/
                    // draggingEntity.
                    if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(waypointEdit.m_Entity))
                    {
                        auto& waypoints = pf.Value().get().m_Waypoints;
                        int const hitIndex = PickWaypointAt(videoSys.GetRenderer(), waypoints, screenPos);
                        if (hitIndex >= 0)
                        {
                            draggingWaypointIndex = hitIndex;
                        }
                        else
                        {
                            auto const worldPos = asge::video::ScreenToWorld(
                                videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(), screenPos);
                            waypoints.push_back(worldPos);
                            RebuildPath(pf.Value().get());
                            MarkActiveSceneDirty(currentProject);
                        }
                    }
                }
                else if (colliderDraw.m_Active)
                {
                    // Phase 12 step 4: starts the drag at the click point,
                    // with a zero-size shape immediately so there's visible
                    // feedback even before any motion -- SDL_EVENT_MOUSE_MOTION
                    // below grows it as the mouse moves.
                    colliderDragging = true;
                    colliderDragStartWorld = asge::video::ScreenToWorld(
                        videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(), screenPos);
                    if (auto collider = sceneManager.GetRegistry().GetComponent<Collider>(colliderDraw.m_Entity))
                    {
                        auto const origin = entityOrigin(colliderDraw.m_Entity);
                        auto& bounds = collider.Value().get().m_LocalBounds;
                        if (auto* rect = std::get_if<asge::math::Rect>(&bounds))
                        {
                            rect->m_X = colliderDragStartWorld.x() - origin.x();
                            rect->m_Y = colliderDragStartWorld.y() - origin.y();
                            rect->m_Width = 0.0f;
                            rect->m_Height = 0.0f;
                        }
                        else if (auto* circle = std::get_if<asge::math::Circle>(&bounds))
                        {
                            circle->m_Center = asge::math::Float2{
                                colliderDragStartWorld.x() - origin.x(), colliderDragStartWorld.y() - origin.y() };
                            circle->m_Radius = 0.0f;
                        }
                    }
                }
                else
                {
                    // Gizmo arms take priority over picking a (possibly
                    // different) entity underneath them.
                    auto const hitAxis = HitTestGizmo(
                        videoSys.GetRenderer(), sceneManager.GetRegistry(), selectedEntity, screenPos);
                    if (hitAxis != GizmoAxis::None)
                    {
                        draggingEntity = selectedEntity;
                        dragAxis = hitAxis;
                    }
                    else
                    {
                        // Screen->world via the renderer's own camera/viewport
                        // math (asge::video::ScreenToWorld), not a re-derived inverse.
                        auto const worldPos = asge::video::ScreenToWorld(
                            videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(), screenPos);
                        selectedEntity = PickEntityAt(sceneManager.GetRegistry(), worldPos);

                        // Selecting an entity also grabs it for an unconstrained
                        // free-drag, in case the mouse moves before releasing.
                        draggingEntity = selectedEntity;
                        dragAxis = GizmoAxis::None;
                    }
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
            {
                draggingEntity = asge::ecs::Entity::Null();
                draggingWaypointIndex = -1;
                if (colliderDragging)
                {
                    // One-shot: a completed drag exits "Draw Collider" mode
                    // automatically rather than staying open for another.
                    colliderDragging = false;
                    MarkActiveSceneDirty(currentProject);
                    colliderDraw = ColliderDrawState{};
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION && draggingEntity != asge::ecs::Entity::Null())
            {
                if (auto transformResult = sceneManager.GetRegistry().GetComponent<Transform>(draggingEntity))
                {
                    // xrel/yrel are already a screen-space delta; dividing by
                    // zoom is the same scaling ScreenToWorld applies, without
                    // needing two absolute ScreenToWorld calls to subtract.
                    float const zoom = videoSys.GetRenderer().GetCamera().m_Zoom;
                    auto& t = transformResult.Value().get();
                    if (dragAxis != GizmoAxis::Y) t.m_LocalCoordinates.x() += event.motion.xrel / zoom;
                    if (dragAxis != GizmoAxis::X) t.m_LocalCoordinates.y() += event.motion.yrel / zoom;
                    t.m_Dirty = true;
                    MarkActiveSceneDirty(currentProject);
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION && draggingWaypointIndex >= 0)
            {
                if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(waypointEdit.m_Entity))
                {
                    auto& waypoints = pf.Value().get().m_Waypoints;
                    if (draggingWaypointIndex < static_cast<int>(waypoints.size()))
                    {
                        // Same xrel/yrel/zoom delta as the entity free-drag above.
                        float const zoom = videoSys.GetRenderer().GetCamera().m_Zoom;
                        auto& waypoint = waypoints[static_cast<std::size_t>(draggingWaypointIndex)];
                        waypoint.x() += event.motion.xrel / zoom;
                        waypoint.y() += event.motion.yrel / zoom;
                        RebuildPath(pf.Value().get());
                        MarkActiveSceneDirty(currentProject);
                    }
                }
                else
                {
                    draggingWaypointIndex = -1;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION && colliderDragging)
            {
                if (auto collider = sceneManager.GetRegistry().GetComponent<Collider>(colliderDraw.m_Entity))
                {
                    auto const worldPos = asge::video::ScreenToWorld(
                        videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(),
                        asge::math::Float2{ event.motion.x, event.motion.y });
                    auto const origin = entityOrigin(colliderDraw.m_Entity);
                    auto& bounds = collider.Value().get().m_LocalBounds;
                    if (auto* rect = std::get_if<asge::math::Rect>(&bounds))
                    {
                        float const startX = colliderDragStartWorld.x() - origin.x();
                        float const startY = colliderDragStartWorld.y() - origin.y();
                        float const curX = worldPos.x() - origin.x();
                        float const curY = worldPos.y() - origin.y();
                        rect->m_X = std::min(startX, curX);
                        rect->m_Y = std::min(startY, curY);
                        rect->m_Width = std::abs(curX - startX);
                        rect->m_Height = std::abs(curY - startY);
                    }
                    else if (auto* circle = std::get_if<asge::math::Circle>(&bounds))
                    {
                        float const dx = worldPos.x() - colliderDragStartWorld.x();
                        float const dy = worldPos.y() - colliderDragStartWorld.y();
                        circle->m_Radius = std::sqrt(dx * dx + dy * dy);
                    }
                    MarkActiveSceneDirty(currentProject);
                }
                else
                {
                    colliderDragging = false;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION && panningCamera)
            {
                // Content follows the cursor (the usual "hand tool" feel),
                // so the camera itself moves opposite the drag -- same
                // xrel/zoom scaling as the entity-drag branch above, just
                // applied to the camera's own position instead of an
                // entity's Transform.
                auto& videoRenderer = videoSys.GetRenderer();
                auto camera = videoRenderer.GetCamera();
                camera.m_X -= event.motion.xrel / camera.m_Zoom;
                camera.m_Y -= event.motion.yrel / camera.m_Zoom;
                videoRenderer.SetCamera(camera);
            }
        }

        // Keeps the viewport exactly matching the live window size every
        // frame, origin pinned at (0,0) -- see the resizable-window comment
        // above for why this replaced an SDL_EVENT_WINDOW_RESIZED handler.
        // Cheap to call unconditionally: SetViewport is just a struct
        // assignment plus one SDL_SetRenderViewport call.
        {
            auto const winSize = videoSys.GetWindow().Size();
            videoSys.GetRenderer().SetViewport( asge::video::Viewport{
                0.0f, 0.0f, static_cast<float>( winSize.x() ), static_cast<float>( winSize.y() ) } );
        }

        GET_TIMESYSTEM.Tick();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Results queued by OnFileDialogResult (possibly from another thread)
        // are drained here, once per frame, on the main thread -- nothing
        // touches SceneManager/Registry/vfs/currentProject from the dialog
        // callback itself.
        {
            std::string chosenDir;
            if (DrainFileDialogResult(createProjectFolderDialogResult, chosenDir))
            {
                if (!chosenDir.empty()) createProjectFolder = chosenDir;
            }
        }
        {
            std::string chosenPath;
            if (DrainFileDialogResult(saveProjectAsDialogResult, chosenPath))
            {
                fs::path path = chosenPath;
                if (path.filename().empty())
                {
                    LOG_WARNING("Save As dialog returned no filename -- project not saved");
                }
                else if (currentProject)
                {
                    if (path.extension().empty()) path += ".asgeproject";
                    fs::path const newDir = path.parent_path();

                    // The active scene is the only one that can have live
                    // unsaved edits (nothing inactive is ever mutable
                    // through the UI) -- save it straight to its new
                    // location; every other scene's on-disk file is already
                    // current, so just copy it there instead.
                    for (std::size_t i = 0; i < currentProject->m_Scenes.size(); ++i)
                    {
                        auto& scene = currentProject->m_Scenes[i];
                        fs::path const newScenePath = newDir / scene.m_Path.filename();
                        bool ok = true;
                        if (static_cast<int>(i) == currentProject->m_ActiveSceneIndex)
                        {
                            auto const r = sceneManager.SaveScene(newScenePath);
                            if (!r) { r.LogError(); ok = false; }
                            else
                            {
                                scene.m_Dirty = false;
                                sceneManager.RenameActiveScene(newScenePath.string());
                            }
                        }
                        else
                        {
                            auto const r = asge::filesystem::Copy(scene.m_Path, newScenePath);
                            if (!r) { r.LogError(); ok = false; }
                        }
                        if (ok) scene.m_Path = newScenePath;
                    }

                    currentProject->m_FilePath = path;
                    auto const saveResult = SaveProject(
                        vfs, sceneManager.GetRegistry(), *currentProject, gridSpacing, targetGameWidth, targetGameHeight);
                    if (!saveResult) saveResult.LogError();
                    else LOG_INFO("Project saved to ", path.string());
                    UpdateWindowTitle(window, &*currentProject);
                }
            }
        }
        {
            std::string chosenPath;
            if (DrainFileDialogResult(openProjectDialogResult, chosenPath))
            {
                if (chosenPath.empty())
                {
                    LOG_WARNING("Open dialog returned no file -- nothing loaded");
                }
                else
                {
                    fs::path const path = chosenPath;

                    // Save whatever's currently open first, same as Create
                    // a Project.
                    if (currentProject)
                    {
                        if (currentProject->m_ActiveSceneIndex >= 0)
                        {
                            auto& active = currentProject->m_Scenes[currentProject->m_ActiveSceneIndex];
                            if (active.m_Dirty)
                            {
                                auto const r = sceneManager.SaveScene(active.m_Path);
                                if (r) active.m_Dirty = false; else r.LogError();
                            }
                        }
                        auto const r = SaveProject(
                            vfs, sceneManager.GetRegistry(), *currentProject, gridSpacing, targetGameWidth, targetGameHeight);
                        if (!r) r.LogError();
                    }
                    sceneManager.UnloadScene();
                    sceneManager.ClearCache();

                    Project loaded;
                    auto const loadResult = LoadProject(vfs, path, loaded, gridSpacing, targetGameWidth, targetGameHeight);
                    if (!loadResult)
                    {
                        loadResult.LogError();
                    }
                    else
                    {
                        currentProject = std::move(loaded);
                        selectedEntity = asge::ecs::Entity::Null();
                        ResetEntityDisplayIds();
                        LOG_INFO("Project loaded from ", path.string());
                        UpdateWindowTitle(window, &*currentProject);
                        if (!currentProject->m_Scenes.empty())
                        {
                            SwitchToScene(
                                *currentProject, 0, sceneManager, assets, videoSys.GetRenderer(), selectedEntity);
                        }
                    }
                }
            }
        }
        {
            std::string chosenPath;
            if (DrainFileDialogResult(openSceneDialogResult, chosenPath))
            {
                if (chosenPath.empty())
                {
                    LOG_WARNING("Open Scene dialog returned no file -- nothing loaded");
                }
                else if (currentProject)
                {
                    fs::path const path = chosenPath;
                    int existingIndex = -1;
                    for (std::size_t i = 0; i < currentProject->m_Scenes.size(); ++i)
                    {
                        if (currentProject->m_Scenes[i].m_Path == path) { existingIndex = static_cast<int>(i); break; }
                    }
                    if (existingIndex < 0)
                    {
                        currentProject->m_Scenes.push_back(ProjectScene{ path.stem().string(), path, false });
                        existingIndex = static_cast<int>(currentProject->m_Scenes.size()) - 1;
                    }
                    SwitchToScene(
                        *currentProject, existingIndex, sceneManager, assets, videoSys.GetRenderer(), selectedEntity);
                    LOG_INFO("Scene opened from ", path.string());
                }
            }
        }

        bool const hasProject = currentProject.has_value();
        bool const hasActiveScene = hasProject
            && currentProject->m_ActiveSceneIndex >= 0
            && currentProject->m_ActiveSceneIndex < static_cast<int>(currentProject->m_Scenes.size());

        bool openCreateProjectModal = false;
        bool openCreateSceneModal = false;
        bool openCreateProjectFolderDialog = false;
        bool openSaveProjectAsDialog = false;
        bool openOpenProjectDialog = false;
        bool openOpenSceneDialog = false;
        bool openGridModal = false;
        bool openGameWindowModal = false;
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::BeginMenu("New"))
                {
                    if (ImGui::MenuItem("Create a Project...")) openCreateProjectModal = true;
                    if (!hasProject) ImGui::BeginDisabled();
                    if (ImGui::MenuItem("Create a Scene...")) openCreateSceneModal = true;
                    if (!hasProject) ImGui::EndDisabled();
                    ImGui::EndMenu();
                }
                ImGui::Separator();

                if (!hasProject) ImGui::BeginDisabled();
                // A project always has a real m_FilePath the moment it
                // exists (Create a Project auto-saves it immediately), so
                // unlike Scene's Save this never needs a Save-As fallback.
                if (ImGui::MenuItem("Save", "Ctrl+P")) saveProjectNow();
                if (ImGui::MenuItem("Save As...")) openSaveProjectAsDialog = true;
                if (!hasProject) ImGui::EndDisabled();

                ImGui::Separator();
                if (!hasActiveScene) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveActiveSceneNow();
                if (!hasActiveScene) ImGui::EndDisabled();

                ImGui::Separator();
                if (ImGui::MenuItem("Open...")) openOpenProjectDialog = true;
                if (!hasProject) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Open Scene...")) openOpenSceneDialog = true;
                if (!hasProject) ImGui::EndDisabled();

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                // Grid spacing/target window size are project-level [View]
                // data (see SaveProject) -- nowhere for either to actually
                // persist to without an active project.
                if (!hasProject) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Grid")) openGridModal = true;
                if (ImGui::MenuItem("Game Window")) openGameWindowModal = true;
                if (!hasProject) ImGui::EndDisabled();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // OpenPopup deferred to here (same ID-stack depth BeginPopupModal
        // itself is called at) rather than issued from inside a menu above
        // -- calling it from inside BeginMenu/EndMenu hashes to a different
        // ID and the popup silently never opens, same gotcha the native
        // dialogs below avoid via their own openXDialog bools.
        if (openGridModal)
        {
            draftGridSpacing = gridSpacing;
            ImGui::OpenPopup("Grid Settings");
        }
        if (openGameWindowModal)
        {
            draftTargetWidth = targetGameWidth;
            draftTargetHeight = targetGameHeight;
            ImGui::OpenPopup("Game Window Settings");
        }
        if (openCreateProjectModal)
        {
            createProjectNameBuf[0] = '\0';
            createProjectFolder.clear();
            createProjectGrid = gridSpacing;
            createProjectWidth = targetGameWidth;
            createProjectHeight = targetGameHeight;
            ImGui::OpenPopup("Create a Project");
        }
        if (openCreateSceneModal && currentProject)
        {
            createSceneNameBuf[0] = '\0';
            createScenePlaceholder = "Scene #" + std::to_string(currentProject->m_Scenes.size());
            ImGui::OpenPopup("Create a Scene");
        }

        if (ImGui::BeginPopupModal("Grid Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::DragFloat("Grid Spacing", &draftGridSpacing, 1.0f, 5.0f, 500.0f);
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup(); // discards the draft
            ImGui::SameLine();
            if (ImGui::Button("Apply"))
            {
                gridSpacing = draftGridSpacing;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("Game Window Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::DragInt("Target Width", &draftTargetWidth, 1.0f, 64, 7680);
            ImGui::DragInt("Target Height", &draftTargetHeight, 1.0f, 64, 4320);
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup(); // discards the draft
            ImGui::SameLine();
            if (ImGui::Button("Apply"))
            {
                targetGameWidth = draftTargetWidth;
                targetGameHeight = draftTargetHeight;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("Create a Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputTextWithHint(
                "Project Name", "New Project", createProjectNameBuf, sizeof(createProjectNameBuf));
            ImGui::Text(
                "Folder: %s", createProjectFolder.empty() ? "(none)" : createProjectFolder.string().c_str());
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) openCreateProjectFolderDialog = true;
            ImGui::DragFloat("Grid Size", &createProjectGrid, 1.0f, 5.0f, 500.0f);
            ImGui::DragInt("Game Window Width", &createProjectWidth, 1.0f, 64, 7680);
            ImGui::DragInt("Game Window Height", &createProjectHeight, 1.0f, 64, 4320);

            bool const canCreate = !createProjectFolder.empty();
            if (!canCreate) ImGui::BeginDisabled();
            if (ImGui::Button("Create"))
            {
                std::string const name =
                    createProjectNameBuf[0] != '\0' ? std::string(createProjectNameBuf) : std::string("New Project");

                // Save whatever project is currently open first -- its
                // dirty scene (there can be at most one, the active one)
                // plus the project file itself.
                if (currentProject)
                {
                    if (currentProject->m_ActiveSceneIndex >= 0)
                    {
                        auto& active = currentProject->m_Scenes[currentProject->m_ActiveSceneIndex];
                        if (active.m_Dirty)
                        {
                            auto const r = sceneManager.SaveScene(active.m_Path);
                            if (r) active.m_Dirty = false; else r.LogError();
                        }
                    }
                    auto const r = SaveProject(
                        vfs, sceneManager.GetRegistry(), *currentProject, gridSpacing, targetGameWidth, targetGameHeight);
                    if (!r) r.LogError();
                }

                // Clear out the entire editor.
                sceneManager.UnloadScene();
                sceneManager.ClearCache();
                auto const mountsCopy = vfs.ListMounts();
                for (auto const& mount : mountsCopy)
                {
                    if (auto r = vfs.Unmount(mount.m_VirtualRoot, mount.m_RealDirectory.string()); !r) r.LogError();
                }
                ClearKnownAssets();
                selectedEntity = asge::ecs::Entity::Null();
                selectedAsset = AssetPick{};
                ResetEntityDisplayIds();

                gridSpacing = createProjectGrid;
                targetGameWidth = createProjectWidth;
                targetGameHeight = createProjectHeight;

                Project newProject;
                newProject.m_FilePath = createProjectFolder / (name + ".asgeproject");
                currentProject = std::move(newProject);

                // Every fresh project starts with one scene, not none.
                CreateSceneInProject(*currentProject, "Empty Scene", sceneManager);
                selectedEntity = asge::ecs::Entity::Null();
                ResetEntityDisplayIds();

                auto const saveResult = SaveProject(
                    vfs, sceneManager.GetRegistry(), *currentProject, gridSpacing, targetGameWidth, targetGameHeight);
                if (!saveResult) saveResult.LogError();
                else LOG_INFO("Project created at ", currentProject->m_FilePath.string());
                UpdateWindowTitle(window, &*currentProject);

                ImGui::CloseCurrentPopup();
            }
            if (!canCreate) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("Create a Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (currentProject)
            {
                ImGui::InputTextWithHint(
                    "Scene Name", createScenePlaceholder.c_str(), createSceneNameBuf, sizeof(createSceneNameBuf));
                if (ImGui::Button("Create"))
                {
                    std::string const name =
                        createSceneNameBuf[0] != '\0' ? std::string(createSceneNameBuf) : createScenePlaceholder;
                    CreateSceneInProject(*currentProject, name, sceneManager);
                    selectedEntity = asge::ecs::Entity::Null();
                    ResetEntityDisplayIds();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
            }
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (openCreateProjectFolderDialog)
        {
            SDL_ShowOpenFolderDialog(OnFileDialogResult, &createProjectFolderDialogResult, window, nullptr, false);
        }
        if (openSaveProjectAsDialog)
        {
            auto const defaultLocation = currentProject
                ? DialogDefaultLocation(currentProject->m_FilePath.parent_path()) : std::string{};
            SDL_ShowSaveFileDialog(
                OnFileDialogResult, &saveProjectAsDialogResult, window,
                kProjectFileFilters, 1, defaultLocation.empty() ? nullptr : defaultLocation.c_str());
        }
        if (openOpenProjectDialog)
        {
            SDL_ShowOpenFileDialog(
                OnFileDialogResult, &openProjectDialogResult, window, kProjectFileFilters, 1, nullptr, false);
        }
        if (openOpenSceneDialog)
        {
            auto const defaultLocation = currentProject
                ? DialogDefaultLocation(currentProject->m_FilePath.parent_path()) : std::string{};
            SDL_ShowOpenFileDialog(
                OnFileDialogResult, &openSceneDialogResult, window,
                kSceneFileFilters, 1, defaultLocation.empty() ? nullptr : defaultLocation.c_str(), false);
        }

        // Right-aligned, stacked above Entities/Inspector (see Inspector.hpp's
        // kEditorPanelWidth/kEditorPanelRightMargin) instead of ImGui's
        // default cascade, which left all three overlapping near the corner.
        // Re-snaps to the edge only on an actual resize (lastDisplaySize
        // changing), ImGuiCond_FirstUseEver otherwise -- see
        // lastDisplaySize's own doc comment for why. Size alone stays
        // user-draggable regardless.
        ImVec2 const displaySize = ImGui::GetIO().DisplaySize;
        ImGuiCond const rightPanelAnchorCond =
            ( displaySize.x != lastDisplaySize.x || displaySize.y != lastDisplaySize.y )
            ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        lastDisplaySize = displaySize;

        float const rightX = displaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
        ImGui::SetNextWindowPos(ImVec2(rightX, 30.0f), rightPanelAnchorCond);
        ImGui::SetNextWindowSize(ImVec2(kEditorPanelWidth, 55.0f), ImGuiCond_FirstUseEver);

        // Recomputed fresh here (not reusing the frame-start hasProject/
        // hasActiveScene above) -- Create a Project's own "Create" button
        // (and Open.../Open Scene's drain blocks) can replace currentProject
        // entirely earlier in this same frame, and the frame-start flags
        // would then be stale (computed against the OLD project) for
        // everything drawn after that point, this panel included. Reusing a
        // stale one here once indexed m_Scenes with a leftover valid-
        // looking index into a NEW, still-empty project -- a real vector-
        // subscript-out-of-range crash this same fix already went in for
        // the Project/Scene HUD below; shared by both, and by every panel
        // drawn after this point that gates a button on project/scene
        // existence (Create Entity, Load Asset..., Add mount).
        bool const freshHasProject = currentProject.has_value();
        bool const freshHasActiveScene = currentProject
            && currentProject->m_ActiveSceneIndex >= 0
            && currentProject->m_ActiveSceneIndex < static_cast<int>( currentProject->m_Scenes.size() );

        // Resync the rename field from the active scene's current name only
        // when the active scene itself just changed -- not every frame,
        // which would overwrite whatever's mid-edit.
        if ( freshHasActiveScene && currentProject->m_ActiveSceneIndex != sceneNameBufSyncedIndex )
        {
            std::snprintf( sceneNameBuf, sizeof( sceneNameBuf ), "%s",
                currentProject->m_Scenes[currentProject->m_ActiveSceneIndex].m_Name.c_str() );
            sceneNameBufSyncedIndex = currentProject->m_ActiveSceneIndex;
        }
        else if ( !freshHasActiveScene )
        {
            sceneNameBufSyncedIndex = -2; // force a resync once a scene next becomes active
        }

        ImGui::Begin("Scene");
        ImGui::Text("Scene loaded: %zu entities", sceneManager.GetRegistry().AllEntities().size());
        if ( freshHasActiveScene )
        {
            ImGui::InputText( "Scene Name", sceneNameBuf, sizeof( sceneNameBuf ) );
            // Committed once editing actually finishes (Enter/Tab/click-
            // away), not per keystroke -- renaming the on-disk file is a
            // real filesystem move, not something to redo on every typed
            // character.
            if ( ImGui::IsItemDeactivatedAfterEdit() )
            {
                auto& active = currentProject->m_Scenes[currentProject->m_ActiveSceneIndex];
                std::string const newName = sceneNameBuf[0] != '\0' ? std::string( sceneNameBuf ) : active.m_Name;

                bool const nameTaken = std::any_of(
                    currentProject->m_Scenes.begin(), currentProject->m_Scenes.end(),
                    [&]( ProjectScene const& inScene ) { return &inScene != &active && inScene.m_Name == newName; } );

                if ( newName == active.m_Name )
                {
                    // No-op edit (typed back to the same name, or cleared
                    // and fell back to it) -- nothing to rename.
                }
                else if ( nameTaken )
                {
                    LOG_ERROR( "\"", newName, "\" is already a scene in this project -- not renamed" );
                    std::snprintf( sceneNameBuf, sizeof( sceneNameBuf ), "%s", active.m_Name.c_str() );
                }
                else
                {
                    fs::path const newPath = active.m_Path.parent_path() / ( newName + ".asgescene" );
                    std::error_code ec;
                    fs::rename( active.m_Path, newPath, ec ); // an actual move, not FileIO::Copy -- no leftover old file
                    if ( ec )
                    {
                        LOG_ERROR( "Failed to rename scene file to \"", newPath.string(), "\": ", ec.message() );
                        std::snprintf( sceneNameBuf, sizeof( sceneNameBuf ), "%s", active.m_Name.c_str() );
                    }
                    else
                    {
                        active.m_Name = newName;
                        active.m_Path = newPath;
                        sceneManager.RenameActiveScene( newPath.string() ); // keep SceneManager's own identity in sync
                        LOG_INFO( "Scene renamed to ", newPath.string() );
                    }
                }
            }

            auto& activeScene = currentProject->m_Scenes[currentProject->m_ActiveSceneIndex];
            ImGui::BeginDisabled( activeScene.m_IsMain );
            if ( ImGui::Button( "Set Main Scene" ) )
            {
                for ( auto& scene : currentProject->m_Scenes ) scene.m_IsMain = false;
                activeScene.m_IsMain = true;
            }
            ImGui::EndDisabled();
            if ( activeScene.m_IsMain ) { ImGui::SameLine(); ImGui::TextUnformatted( "Main scene" ); }
        }
        ImGui::End();

        // Four fixed HUDs, not regular panels -- no title bar/move/
        // collapse/close, repositioned every frame (ImGuiCond_Always, not
        // FirstUseEver) so none can be dragged away, hidden behind another
        // window, or lost the way a field inside a closable/collapsible
        // panel like Scene could be. Each is placed flush against the
        // previous one's own right edge (GetWindowPos/GetWindowSize read
        // right before each End()), and the whole row is anchored off
        // hudGroupWidth so the group as a whole sits centered rather than
        // the first panel alone.
        float const hudGroupStartX = ImGui::GetIO().DisplaySize.x * 0.5f - hudGroupWidth * 0.5f;
        constexpr float kHudGap = 8.0f;

        // Project/Scene HUD -- prepended ahead of the mouse-position HUD.
        // Needs real input (the Scene combo), so unlike the two pure-
        // display HUDs after it, no NoInputs.

        ImVec2 projectHudPos, projectHudSize;
        {
            ImGuiWindowFlags const kProjectHudFlags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
              | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
              | ImGuiWindowFlags_NoMove;

            ImGui::SetNextWindowPos( ImVec2( hudGroupStartX, ImGui::GetFrameHeight() + 4.0f ), ImGuiCond_Always );
            ImGui::SetNextWindowBgAlpha( 0.55f );
            ImGui::Begin( "##ProjectHud", nullptr, kProjectHudFlags );

            if ( !currentProject )
            {
                ImGui::TextUnformatted( "Project: (none)" );
            }
            else
            {
                ImGui::Text( "Project: %s", currentProject->m_FilePath.stem().string().c_str() );
                ImGui::SameLine();

                std::string const currentSceneName = freshHasActiveScene
                    ? currentProject->m_Scenes[currentProject->m_ActiveSceneIndex].m_Name : std::string( "(none)" );
                ImGui::SetNextItemWidth( 150.0f );
                if ( ImGui::BeginCombo( "Scene", currentSceneName.c_str() ) )
                {
                    for ( std::size_t i = 0; i < currentProject->m_Scenes.size(); ++i )
                    {
                        bool const isSelected = static_cast<int>( i ) == currentProject->m_ActiveSceneIndex;
                        if ( ImGui::Selectable( currentProject->m_Scenes[i].m_Name.c_str(), isSelected ) && !isSelected )
                        {
                            SwitchToScene(
                                *currentProject, static_cast<int>( i ),
                                sceneManager, assets, videoSys.GetRenderer(), selectedEntity );
                        }
                    }
                    ImGui::EndCombo();
                }

                // A white-filled dot, shown only while the active scene has
                // unsaved edits.
                if ( freshHasActiveScene && currentProject->m_Scenes[currentProject->m_ActiveSceneIndex].m_Dirty )
                {
                    ImGui::SameLine();
                    float const radius = 5.0f;
                    ImVec2 const cursor = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        ImVec2( cursor.x + radius, cursor.y + ImGui::GetTextLineHeight() * 0.5f ),
                        radius, IM_COL32( 255, 255, 255, 255 ) );
                    ImGui::Dummy( ImVec2( radius * 2.0f + 4.0f, ImGui::GetTextLineHeight() ) );
                }
            }

            projectHudPos = ImGui::GetWindowPos();
            projectHudSize = ImGui::GetWindowSize();
            ImGui::End();
        }

        ImVec2 mouseHudPos, mouseHudSize;
        {
            ImGuiWindowFlags const kMouseHudFlags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
              | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
              | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

            ImGui::SetNextWindowPos(
                ImVec2( projectHudPos.x + projectHudSize.x + kHudGap, projectHudPos.y ), ImGuiCond_Always );
            ImGui::SetNextWindowBgAlpha( 0.55f );
            ImGui::Begin( "##MouseHud", nullptr, kMouseHudFlags );

            // Screen (window-relative pixels, what mouse events report) and
            // world (via the same ScreenToWorld the viewport picking/gizmo
            // code already uses) -- both, since which one's useful depends
            // on whether you're placing something by eye or matching a
            // Transform's own numbers in the Inspector.
            ImVec2 const mouseScreen = ImGui::GetIO().MousePos;
            auto const mouseWorld = asge::video::ScreenToWorld(
                videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(),
                asge::math::Float2{ mouseScreen.x, mouseScreen.y } );
            ImGui::Text( "Mouse: screen (%.0f, %.0f)  world (%.1f, %.1f)",
                mouseScreen.x, mouseScreen.y, mouseWorld.x(), mouseWorld.y() );

            mouseHudPos = ImGui::GetWindowPos();
            mouseHudSize = ImGui::GetWindowSize();
            ImGui::End();
        }
        ImVec2 viewHudPos, viewHudSize;
        {
            ImGuiWindowFlags const kViewHudFlags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
              | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
              | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

            ImGui::SetNextWindowPos(
                ImVec2( mouseHudPos.x + mouseHudSize.x + kHudGap, mouseHudPos.y ), ImGuiCond_Always );
            ImGui::SetNextWindowBgAlpha( 0.55f );
            ImGui::Begin( "##ViewSettingsHud", nullptr, kViewHudFlags );

            ImGui::Text( "Grid: %.0f", gridSpacing );
            ImGui::SameLine();
            ImGui::Text( "Game Window: %dx%d", targetGameWidth, targetGameHeight );

            viewHudPos = ImGui::GetWindowPos();
            viewHudSize = ImGui::GetWindowSize();
            ImGui::End();
        }
        {
            // Its own panel rather than living inside ##ViewSettingsHud --
            // a real button, so (unlike the pure-display HUDs either side
            // of it) this one can't be NoInputs.
            ImGuiWindowFlags const kRestoreViewFlags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
              | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
              | ImGuiWindowFlags_NoMove;

            ImGui::SetNextWindowPos(
                ImVec2( viewHudPos.x + viewHudSize.x + kHudGap, viewHudPos.y ), ImGuiCond_Always );
            ImGui::SetNextWindowBgAlpha( 0.55f );
            // NoDecoration drops the title bar/resize grip/scrollbar but not
            // ImGui's own window border, which is a separate style var --
            // pushed to 0 here (just this one window) since it'd otherwise
            // outline the button in a thin rect that reads as a second,
            // redundant border around it.
            ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
            ImGui::Begin( "##RestoreViewHud", nullptr, kRestoreViewFlags );

            // Reset pan/zoom back to Camera's own defaults (origin, 1x) --
            // the one way back to a known view once middle-drag/scroll has
            // wandered off, short of eyeballing values back by hand.
            if (ImGui::Button("Restore View")) videoSys.GetRenderer().SetCamera(asge::video::Camera{});

            ImVec2 const restoreViewPos = ImGui::GetWindowPos();
            ImVec2 const restoreViewSize = ImGui::GetWindowSize();
            ImGui::End();
            ImGui::PopStyleVar();

            // Feeds next frame's hudGroupStartX -- see its own comment above.
            hudGroupWidth = ( restoreViewPos.x + restoreViewSize.x ) - hudGroupStartX;
        }

        // Phase 3: entity list panel drives the same selection state as
        // viewport picking (Phase 2) -- one selection state, two input paths.
        auto const entityListResult = DrawEntityListPanel(sceneManager.GetRegistry(), selectedEntity, freshHasProject);
        if (entityListResult.m_CreateClicked)
        {
            // Phase 5: "Create entity" goes through the same Registry::
            // CreateEntity() + AddComponent() gameplay code uses -- no
            // separate editor-only construction API. A bare Transform is
            // the minimum needed for the new entity to be visible/pickable
            // here; SceneId tags it into the active scene so Save doesn't
            // silently drop it (see DuplicateEntity's own comment).
            auto created = sceneManager.GetRegistry().CreateEntity();
            if (created)
            {
                sceneManager.GetRegistry().AddComponent<Transform>(created.Value(), Transform{});
                if (auto const& path = sceneManager.CurrentScenePath())
                {
                    sceneManager.GetRegistry().AddComponent<asge::game::scene::SceneId>(
                        created.Value(), asge::game::scene::SceneId{*path});
                }
                selectedEntity = created.Value();
                MarkActiveSceneDirty(currentProject);
            }
            else created.LogError();
        }

        // Phase 16: Create UI Element -- Type Selection -> Creation, with a
        // Back button on the latter returning to the former. Every OpenPopup
        // call below sits at this same top-level ID-stack depth (never
        // issued from inside another popup's own Begin/EndPopupModal block)
        // -- see the Grid/Game Window modals' own comment above for what
        // silently breaks otherwise; a Back/Cancel-triggered reopen is
        // consumed on the following frame instead; same one-frame lag as
        // hudGroupWidth above, imperceptible here too.
        auto const resetUICreateDraft = [&](UIElementType inType) noexcept
        {
            uiCreateType = inType;
            uiCreateNameBuf[0] = '\0';
            uiCreatePos[0] = uiCreatePos[1] = 0.0f;
            uiCreateSize[0] = DefaultUISize(inType).x();
            uiCreateSize[1] = DefaultUISize(inType).y();
            uiCreateAutoSize = true;
            uiCreateScreenSpace = true;
            uiCreateEnabled = true;
            uiCreateTextBuf[0] = '\0';
            uiCreateFontPath.clear();
            uiCreateFontPixelHeight = asge::game::ui::consts::kFontPixelHeight;
            uiCreateAlign = static_cast<int>(asge::str::TextAlign::Left);
            uiCreateVAlign = static_cast<int>(asge::game::components::VerticalAlign::Center);
            FloatFromColor(asge::game::ui::consts::kDefaultColor, uiCreateTextColor);
            FloatFromColor(asge::game::ui::consts::kStateColor.m_Color, uiCreateButtonColor);
            FloatFromColor(asge::game::ui::consts::kStateColor.m_HoverColor, uiCreateButtonHoverColor);
            FloatFromColor(asge::game::ui::consts::kStateColor.m_PressedColor, uiCreateButtonPressedColor);

            uiCreateChecked = false;
            FloatFromColor(asge::game::ui::consts::kDefaultColor, uiCreateCheckColor);
            uiCreateMin = 0.0f; uiCreateMax = 1.0f; uiCreateValue = 0.0f;
            FloatFromColor(asge::graphics::colors::s_Gray, uiCreateTrackColor);
            FloatFromColor(asge::graphics::colors::s_ShadowBlack, uiCreateBackground);
            uiCreateBorder = true;
            FloatFromColor(asge::graphics::colors::s_LightGray, uiCreateBorderColor);
            uiCreateMargin[0] = uiCreateMargin[1] = 0.0f;
            uiCreatePadding[0] = uiCreatePadding[1] = 0.0f;
            uiCreateSpacing[0] = uiCreateSpacing[1] = 0.0f;
            uiCreateLayout = static_cast<int>(asge::game::components::PanelLayout::Absolute);
            uiCreateLayoutRows = uiCreateLayoutCols = 3;
            uiCreateParent = asge::ecs::Entity::Null();
        };

        bool wantOpenUICreate = false;
        if (entityListResult.m_CreateUIElementClicked) ImGui::OpenPopup("Select UI Element Type");

        if (ImGui::BeginPopupModal("Select UI Element Type", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::Selectable("Button"))
            {
                resetUICreateDraft(UIElementType::Button);
                wantOpenUICreate = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Label"))
            {
                resetUICreateDraft(UIElementType::Label);
                wantOpenUICreate = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Checkbox"))
            {
                resetUICreateDraft(UIElementType::Checkbox);
                wantOpenUICreate = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Slider"))
            {
                resetUICreateDraft(UIElementType::Slider);
                wantOpenUICreate = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Panel"))
            {
                resetUICreateDraft(UIElementType::Panel);
                wantOpenUICreate = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (wantOpenUICreate) ImGui::OpenPopup("Create UI Element");

        bool wantOpenUIType = false;
        if (ImGui::BeginPopupModal("Create UI Element", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            bool const isButton = uiCreateType == UIElementType::Button;
            bool const isLabel = uiCreateType == UIElementType::Label;
            bool const isCheckbox = uiCreateType == UIElementType::Checkbox;
            bool const isSlider = uiCreateType == UIElementType::Slider;
            bool const isPanel = uiCreateType == UIElementType::Panel;
            static char const* const kTypeNames[]{ "Button", "Label", "Checkbox", "Slider", "Panel" };
            ImGui::Text("Type: %s", kTypeNames[static_cast<int>(uiCreateType)]);
            ImGui::InputTextWithHint("Name", "(unnamed)", uiCreateNameBuf, sizeof(uiCreateNameBuf));
            ImGui::DragFloat2("Position", uiCreatePos);
            ImGui::Checkbox("Screen Space", &uiCreateScreenSpace);

            if (isLabel)
            {
                ImGui::Checkbox("Auto Size", &uiCreateAutoSize);
                if (!uiCreateAutoSize) ImGui::DragFloat2("Size", uiCreateSize, 1.0f, 1.0f, 4096.0f);
            }
            else
            {
                ImGui::DragFloat2("Size", uiCreateSize, 1.0f, 1.0f, 4096.0f);
            }

            if (isButton || isCheckbox || isSlider)
            {
                ImGui::Checkbox("Enabled", &uiCreateEnabled);
                // Button state colors / Checkbox box colors / Slider thumb colors
                ImGui::ColorEdit4(isSlider ? "Thumb Color" : "Color", uiCreateButtonColor);
                ImGui::ColorEdit4(isSlider ? "Thumb Hover Color" : "Hover Color", uiCreateButtonHoverColor);
                ImGui::ColorEdit4(isSlider ? "Thumb Pressed Color" : "Pressed Color", uiCreateButtonPressedColor);
            }

            if (isCheckbox)
            {
                ImGui::Checkbox("Checked", &uiCreateChecked);
                ImGui::ColorEdit4("Check Color", uiCreateCheckColor);
            }

            if (isSlider)
            {
                ImGui::DragFloat("Min", &uiCreateMin, 0.05f);
                ImGui::DragFloat("Max", &uiCreateMax, 0.05f);
                ImGui::SliderFloat("Value", &uiCreateValue, uiCreateMin, uiCreateMax);
                ImGui::ColorEdit4("Track Color", uiCreateTrackColor);
            }

            if (isPanel)
            {
                ImGui::ColorEdit4("Background", uiCreateBackground);
                ImGui::Checkbox("Border", &uiCreateBorder);
                if (uiCreateBorder) ImGui::ColorEdit4("Border Color", uiCreateBorderColor);
                ImGui::DragFloat2("Margin", uiCreateMargin, 0.5f, 0.0f, 4096.0f);
                ImGui::DragFloat2("Padding", uiCreatePadding, 0.5f, 0.0f, 4096.0f);
                ImGui::DragFloat2("Spacing", uiCreateSpacing, 0.5f, 0.0f, 4096.0f);
                static char const* const kLayoutNames[]{ "Absolute", "Grid", "VStack", "HStack" };
                ImGui::Combo("Layout", &uiCreateLayout, kLayoutNames, 4);
                auto const layout = static_cast<asge::game::components::PanelLayout>(uiCreateLayout);
                if (layout == asge::game::components::PanelLayout::Grid)
                {
                    ImGui::DragInt("Rows", &uiCreateLayoutRows, 0.1f, 1, 64);
                    ImGui::DragInt("Columns", &uiCreateLayoutCols, 0.1f, 1, 64);
                }
                else if (layout == asge::game::components::PanelLayout::VStack)
                {
                    ImGui::DragInt("Rows", &uiCreateLayoutRows, 0.1f, 1, 64);
                }
                else if (layout == asge::game::components::PanelLayout::HStack)
                {
                    ImGui::DragInt("Columns", &uiCreateLayoutCols, 0.1f, 1, 64);
                }
            }

            // Phase 17: Panel Parent -- any widget (a Panel included, for
            // nesting) can be created as a child of an existing panel, which
            // then lays it out (systems::UILayoutSystem).
            ImGui::Separator();
            ImGui::TextUnformatted("Panel Parent");
            {
                std::vector<asge::ecs::Entity> panels;
                for (auto [entity, panel] : sceneManager.GetRegistry().View<asge::game::components::UIPanel>())
                {
                    (void)panel;
                    panels.push_back(entity);
                }
                std::sort(panels.begin(), panels.end(), [&](asge::ecs::Entity inA, asge::ecs::Entity inB)
                {
                    return GetEntityLabel(sceneManager.GetRegistry(), inA) < GetEntityLabel(sceneManager.GetRegistry(), inB);
                });

                std::string const currentLabel = uiCreateParent == asge::ecs::Entity::Null()
                    ? std::string("(none)") : GetEntityLabel(sceneManager.GetRegistry(), uiCreateParent);
                if (ImGui::BeginCombo("Parent", currentLabel.c_str()))
                {
                    if (ImGui::Selectable("(none)", uiCreateParent == asge::ecs::Entity::Null()))
                    {
                        uiCreateParent = asge::ecs::Entity::Null();
                    }
                    for (auto panelEntity : panels)
                    {
                        ImGui::PushID(static_cast<int>(panelEntity.m_Index));
                        if (ImGui::Selectable(GetEntityLabel(sceneManager.GetRegistry(), panelEntity).c_str(),
                                              uiCreateParent == panelEntity))
                        {
                            uiCreateParent = panelEntity;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                if (panels.empty()) ImGui::TextDisabled("No panels in the scene yet.");
            }

            if (isButton || isLabel)
            {
                ImGui::Separator();
                ImGui::TextUnformatted(isButton ? "Button Text (optional)" : "Text");
                ImGui::InputText("Content", uiCreateTextBuf, sizeof(uiCreateTextBuf));
                // Font is a loadable asset like Texture/Audio/Animation (Phase
                // 16) -- picked from what's known to be loaded, same
                // DrawAssetPathCombo widget Sprite/AudioSource/Animation's own
                // dropdowns use, not a hand-typed path.
                DrawAssetPathCombo("Font Path", uiCreateFontPath, KnownFontPaths(sceneManager.GetRegistry()));
                ImGui::DragInt("Font Size", &uiCreateFontPixelHeight, 1.0f, 1, 256);
                if (ImGui::IsItemHovered())
                {
                    auto const atlasSize = asge::media::Font::GetAtlasSize();
                    ImGui::SetTooltip(
                        "Every font bakes into a fixed %dx%d atlas -- too large a size for a "
                        "given font's own glyphs to all fit fails to resolve.", atlasSize.x(), atlasSize.y());
                }
                static char const* const kAlignNames[]{ "None", "Left", "Center", "Right" };
                ImGui::Combo("Align", &uiCreateAlign, kAlignNames, 4);
                static char const* const kVAlignNames[]{ "Top", "Center", "Bottom" };
                ImGui::Combo("Vertical Align", &uiCreateVAlign, kVAlignNames, 3);
                ImGui::ColorEdit4("Text Color", uiCreateTextColor);
            }

            ImGui::Separator();
            if (ImGui::Button("Back"))
            {
                wantOpenUIType = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard")) ImGui::CloseCurrentPopup();
            ImGui::SameLine();
            if (ImGui::Button("Create"))
            {
                asge::game::ui::TextDesc text;
                text.m_Content = uiCreateTextBuf;
                text.m_FontPath = uiCreateFontPath;
                text.m_FontPixelHeight = uiCreateFontPixelHeight;
                text.m_Align = static_cast<asge::str::TextAlign>(uiCreateAlign);
                text.m_VerticalAlign = static_cast<asge::game::components::VerticalAlign>(uiCreateVAlign);
                text.m_Color = ColorFromFloat4(uiCreateTextColor);

                // Phase 16: MUST go through UI.hpp's own Create*, not a
                // hand-rolled AddComponent sequence -- these are the exact
                // functions gameplay code uses to spawn a Button/Label.
                auto& registry = sceneManager.GetRegistry();
                asge::math::Float2 const position{ uiCreatePos[0], uiCreatePos[1] };
                asge::math::Float2 const size{ uiCreateSize[0], uiCreateSize[1] };
                asge::game::components::details::StateColors const stateColors{
                    ColorFromFloat4(uiCreateButtonColor), ColorFromFloat4(uiCreateButtonHoverColor),
                    ColorFromFloat4(uiCreateButtonPressedColor) };

                asge::Result<asge::ecs::Entity> created = [&]
                {
                    using namespace asge::game::ui;
                    switch (uiCreateType)
                    {
                    case UIElementType::Button:
                        return CreateButton(registry, ButtonDesc{
                            .m_Name = uiCreateNameBuf, .m_Enabled = uiCreateEnabled, .m_Position = position,
                            .m_Size = size, .m_Colors = stateColors, .m_ScreenSpace = uiCreateScreenSpace,
                            .m_Text = text });
                    case UIElementType::Label:
                        return CreateLabel(registry, LabelDesc{
                            .m_Name = uiCreateNameBuf, .m_Position = position,
                            .m_Size = uiCreateAutoSize ? std::nullopt : std::optional<asge::math::Float2>(size),
                            .m_ScreenSpace = uiCreateScreenSpace, .m_Text = text });
                    case UIElementType::Checkbox:
                        return CreateCheckbox(registry, CheckboxDesc{
                            .m_Name = uiCreateNameBuf, .m_Enabled = uiCreateEnabled, .m_Position = position,
                            .m_Size = size, .m_BoxColors = stateColors,
                            .m_CheckColor = ColorFromFloat4(uiCreateCheckColor), .m_Checked = uiCreateChecked,
                            .m_ScreenSpace = uiCreateScreenSpace });
                    case UIElementType::Slider:
                        return CreateSlider(registry, SliderDesc{
                            .m_Name = uiCreateNameBuf, .m_Enabled = uiCreateEnabled, .m_Position = position,
                            .m_Size = size, .m_Min = uiCreateMin, .m_Max = uiCreateMax, .m_Value = uiCreateValue,
                            .m_TrackColor = ColorFromFloat4(uiCreateTrackColor), .m_ThumbColor = stateColors,
                            .m_ScreenSpace = uiCreateScreenSpace });
                    case UIElementType::Panel:
                        break;
                    }

                    using namespace asge::game::components;
                    LayoutSpec layout = LayoutAbsolute{};
                    switch (static_cast<PanelLayout>(uiCreateLayout))
                    {
                    case PanelLayout::Absolute: break;
                    case PanelLayout::Grid: layout = LayoutGrid{ .m_Rows = uiCreateLayoutRows, .m_Cols = uiCreateLayoutCols }; break;
                    case PanelLayout::VStack: layout = LayoutVStack{ .m_Rows = uiCreateLayoutRows }; break;
                    case PanelLayout::HStack: layout = LayoutHStack{ .m_Cols = uiCreateLayoutCols }; break;
                    }
                    return CreatePanel(registry, PanelDesc{
                        .m_Name = uiCreateNameBuf, .m_Position = position, .m_Size = size,
                        .m_Background = ColorFromFloat4(uiCreateBackground), .m_Layout = layout,
                        .m_Padding = { uiCreatePadding[0], uiCreatePadding[1] },
                        .m_Margin = { uiCreateMargin[0], uiCreateMargin[1] },
                        .m_Spacing = { uiCreateSpacing[0], uiCreateSpacing[1] },
                        .m_Border = uiCreateBorder, .m_BorderColor = ColorFromFloat4(uiCreateBorderColor),
                        .m_ScreenSpace = uiCreateScreenSpace });
                }();

                if (created)
                {
                    // Phase 17: Panel Parent -- the chosen panel lays the new
                    // widget out from the next frame on (UILayoutSystem).
                    if (uiCreateParent != asge::ecs::Entity::Null())
                    {
                        AttachChild(registry, uiCreateParent, created.Value());
                    }

                    if (auto const& path = sceneManager.CurrentScenePath())
                    {
                        sceneManager.GetRegistry().AddComponent<asge::game::scene::SceneId>(
                            created.Value(), asge::game::scene::SceneId{*path});
                    }
                    selectedEntity = created.Value();
                    // A freshly-created UILabel/RenderInfo has nothing
                    // resolved yet (m_Font/m_Texture stay null) until this
                    // runs -- same reasoning as
                    // AssetContextAction::CreateEntity/AttachTo above.
                    assets.ResolveAssets(sceneManager.GetRegistry(), videoSys.GetRenderer());
                    MarkActiveSceneDirty(currentProject);
                    ImGui::CloseCurrentPopup();
                }
                else created.LogError(); // e.g. text content with no font path -- left open so it can be fixed
            }
            ImGui::EndPopup();
        }
        if (wantOpenUIType) ImGui::OpenPopup("Select UI Element Type");

        // Phase 13: New Child/Detach/Remove (the Entities tree's row context
        // menu) and Reparent (a row dropped onto another) -- same "goes
        // through the same Registry/Hierarchy calls gameplay code uses" as
        // Create Entity above.
        switch (entityListResult.m_Action)
        {
        case HierarchyAction::NewChild:
        {
            auto created = sceneManager.GetRegistry().CreateEntity();
            if (created)
            {
                sceneManager.GetRegistry().AddComponent<Transform>(created.Value(), Transform{});
                if (auto const& path = sceneManager.CurrentScenePath())
                {
                    sceneManager.GetRegistry().AddComponent<asge::game::scene::SceneId>(
                        created.Value(), asge::game::scene::SceneId{*path});
                }
                AttachChild(sceneManager.GetRegistry(), entityListResult.m_Target, created.Value());
                selectedEntity = created.Value();
                MarkActiveSceneDirty(currentProject);
            }
            else created.LogError();
            break;
        }
        case HierarchyAction::Detach:
            DetachChild(sceneManager.GetRegistry(), entityListResult.m_Target);
            MarkActiveSceneDirty(currentProject);
            break;
        case HierarchyAction::Remove:
        {
            DestroyEntityGraph(sceneManager.GetRegistry(), entityListResult.m_Target);
            // Remove can take out a whole subtree at once -- selectedEntity
            // might have been one of its descendants, not just the
            // right-clicked entity itself, so re-check aliveness rather than
            // only comparing against m_Target (same check DuplicateEntity
            // already does for the same reason).
            auto const alive = sceneManager.GetRegistry().AllEntities();
            if (std::find(alive.begin(), alive.end(), selectedEntity) == alive.end())
            {
                selectedEntity = asge::ecs::Entity::Null();
            }
            MarkActiveSceneDirty(currentProject);
            break;
        }
        case HierarchyAction::Reparent:
            AttachChild(sceneManager.GetRegistry(), entityListResult.m_NewParent, entityListResult.m_Target);
            MarkActiveSceneDirty(currentProject);
            break;
        case HierarchyAction::None:
            break;
        }

        auto const inspectorResult = DrawInspectorPanel(
            sceneManager.GetRegistry(), selectedEntity,
            KnownTexturePaths(sceneManager.GetRegistry()),
            KnownAnimationPaths(sceneManager.GetRegistry()),
            KnownAudioPaths(sceneManager.GetRegistry()),
            KnownFontPaths(sceneManager.GetRegistry()),
            waypointEdit, colliderDraw);

        if (inspectorResult.m_FieldChanged) MarkActiveSceneDirty(currentProject);

        switch (inspectorResult.m_Action)
        {
        case EntityAction::Delete:
            if (auto const destroyResult = sceneManager.GetRegistry().DestroyEntity(selectedEntity); !destroyResult)
            {
                destroyResult.LogError();
            }
            selectedEntity = asge::ecs::Entity::Null();
            break;
        case EntityAction::Duplicate:
            if (auto const& path = sceneManager.CurrentScenePath())
            {
                selectedEntity = DuplicateEntity(sceneManager.GetRegistry(), selectedEntity, *path);
            }
            break;
        case EntityAction::ComponentsChanged:
            // A newly-added Sprite/Animation/AudioSource has nothing
            // resolved yet -- called here rather than unconditionally every
            // frame so a component that's still unresolved (missing mount,
            // bad path) doesn't re-log the same failure every single frame.
            assets.ResolveAssets(sceneManager.GetRegistry(), videoSys.GetRenderer());
            break;
        case EntityAction::None:
            break;
        }

        // Phase 7: browse what's known (scene usage + "Load Asset..."
        // imports) instead of typing/memorizing a virtual path. Clicking an
        // entry no longer assigns it to the selected entity -- it just
        // opens the Asset Inspector below on that entry; a Sprite gets its
        // texture by picking one at Add Component time instead (see
        // Inspector.hpp's DrawInspectorPanel).
        auto const assetBrowserResult = DrawAssetBrowserPanel(sceneManager.GetRegistry(), vfs, window, freshHasProject);
        if (assetBrowserResult.m_Pick.m_Kind != AssetPickKind::None) selectedAsset = assetBrowserResult.m_Pick;
        DrawAssetInspectorPanel(
            selectedAsset, vfs, assets, videoSys.GetRenderer(), audioDevice, sceneManager.GetRegistry(),
            assetBrowserResult.m_OpenCreateClip);

        // Phase 14: an asset row's "Create Entity"/"Attach To" context menu
        // -- same "goes through the same Registry calls gameplay code uses"
        // as Phase 13's New Child, plus the RenderInfo auto-attach for a
        // Sprite (see AttachAssetComponent).
        switch (assetBrowserResult.m_ContextAction)
        {
        case AssetContextAction::CreateEntity:
        {
            auto created = sceneManager.GetRegistry().CreateEntity();
            if (created)
            {
                sceneManager.GetRegistry().AddComponent<Transform>(created.Value(), Transform{});
                AttachAssetComponent(
                    sceneManager.GetRegistry(), created.Value(),
                    assetBrowserResult.m_ContextKind, assetBrowserResult.m_ContextPath);
                if (auto const& path = sceneManager.CurrentScenePath())
                {
                    sceneManager.GetRegistry().AddComponent<asge::game::scene::SceneId>(
                        created.Value(), asge::game::scene::SceneId{*path});
                }
                selectedEntity = created.Value(); // "directly opens the inspector panel" -- Inspector shows whatever's selected
                // A freshly-attached Sprite/Animation/AudioSource has nothing
                // resolved yet (m_Texture etc. stay null) until this runs --
                // same reasoning as EntityAction::ComponentsChanged above.
                assets.ResolveAssets(sceneManager.GetRegistry(), videoSys.GetRenderer());
                MarkActiveSceneDirty(currentProject);
            }
            else created.LogError();
            break;
        }
        case AssetContextAction::AttachTo:
            AttachAssetComponent(
                sceneManager.GetRegistry(), assetBrowserResult.m_ContextTarget,
                assetBrowserResult.m_ContextKind, assetBrowserResult.m_ContextPath);
            selectedEntity = assetBrowserResult.m_ContextTarget;
            assets.ResolveAssets(sceneManager.GetRegistry(), videoSys.GetRenderer());
            MarkActiveSceneDirty(currentProject);
            break;
        case AssetContextAction::None:
            break;
        }

        // Always-on panel: lists/adds VirtualFileSystem mounts, and surfaces
        // any root the current scene's assets reference but isn't mounted --
        // resolves internally right after a successful mount (see its own
        // doc comment), same "call on change, not every frame" reasoning as
        // the ComponentsChanged/asset-browser resolves above.
        DrawVfsPanel(vfs, sceneManager.GetRegistry(), assets, videoSys.GetRenderer(), window, freshHasProject);

        DrawConsolePanel(window);

        // Flushes this frame's gizmo-drag/inspector edits (and any Hierarchy
        // reparenting) from Transform's Local into its World fields -- every
        // overlay below and RenderPipeline itself read World only, and
        // neither one runs this (see examples/*/Game.cpp for the same
        // per-frame call gameplay code has to make of its own accord).
        // Phase 17: panels place/resize their children first, so the same
        // propagation pass below carries the result into World (see
        // UILayoutSystem's own doc comment).
        asge::game::systems::UILayoutSystem(sceneManager.GetRegistry());
        asge::game::systems::TransformPropagationSystem(sceneManager.GetRegistry());

        // Phase 4: viewport overlays, so a position/collider is readable
        // directly off the scene instead of only through the inspector.
        // GetBackgroundDrawList() renders behind every ImGui window/panel,
        // in front of the sprites RenderPipeline draws below.
        DrawWorldGrid(videoSys.GetRenderer(), ImGui::GetBackgroundDrawList(), gridSpacing);
        DrawColliderOverlays(videoSys.GetRenderer(), sceneManager.GetRegistry(), ImGui::GetBackgroundDrawList());
        DrawTranslateGizmo(
            videoSys.GetRenderer(), sceneManager.GetRegistry(), selectedEntity, ImGui::GetBackgroundDrawList());
        DrawGameWindowPreview(
            videoSys.GetRenderer(), ImGui::GetBackgroundDrawList(), targetGameWidth, targetGameHeight);
        DrawCameraOverlays(
            videoSys.GetRenderer(), sceneManager.GetRegistry(), ImGui::GetBackgroundDrawList(),
            targetGameWidth, targetGameHeight);
        // Shown for the selected entity's PathFollow regardless of whether
        // "Select Waypoints" mode is active, so placed waypoints stay
        // visible once selection ends, not just while adding them -- unless
        // the entity is disabled, same as the Collider/Camera overlays.
        if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(selectedEntity);
            pf && !sceneManager.GetRegistry().IsDisabled(selectedEntity))
        {
            DrawPathFollowWaypointOverlay(
                videoSys.GetRenderer(), ImGui::GetBackgroundDrawList(), pf.Value().get().m_Waypoints,
                waypointEdit.m_Active, pf.Value().get().m_Resolution);
        }

        ImGui::Render();

        // The editor hijacks IRenderer's camera for its own free-roam pan/
        // zoom navigation rather than following a components::Camera entity
        // -- without this, RenderSystem's screen-space content would stay
        // glued to the window's own raw corner regardless of where that
        // navigation is currently looking, instead of panning/zooming
        // together with everything else (see ScreenSpaceCamera's own doc
        // comment). CameraSystem never overwrites this camera in the editor
        // (resources::ActiveCamera is never set here), so the value read
        // here is exactly what RenderSystem will see.
        sceneManager.GetRegistry().SetResource(
            asge::game::resources::ScreenSpaceCamera{ videoSys.GetRenderer().GetCamera() });

        videoSys.GetRenderer().Clear({ 15, 15, 20, 255 });
        asge::game::systems::RenderPipeline(
            sceneManager.GetRegistry(), videoSys.GetRenderer(), asge::time::DeltaTime());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        videoSys.GetRenderer().Present();

        sessionAutosaveTimer += asge::time::DeltaTime();
        if (sessionAutosaveTimer >= kSessionAutosaveInterval)
        {
            sessionAutosaveTimer = 0.0f;
            saveEditorSessionNow();
        }
    }

    saveEditorSessionNow(); // one last save on a clean exit, not just periodic

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    return 0;
}

#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/PathFollow.hpp>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Scene/SceneId.hpp>
#include <ASGE/Audio/AudioDevice.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include <algorithm>
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

constexpr SDL_DialogFileFilter kProjectFileFilters[]{ { "Project (*.asgeproject)", "asgeproject" } };
constexpr SDL_DialogFileFilter kSceneFileFilters[]{ { "Scene (*.asgescene)", "asgescene" } };

/**
 * @brief Finds the topmost entity (by iteration order) whose
 *        GetEntityWorldBounds contains inWorldPos, or Entity::Null() if none.
 *
 * No multi-select yet, so the last match wins rather than resolving overlap
 * by draw order.
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
        if ( hit ) picked = entity;
    }
    return picked;
}

/**
 * @brief Copies every serializable component inSource has onto a freshly
 *        created entity, tagged with inScenePath's SceneId so it's included
 *        in ActiveEntities()/SaveScene() alongside everything else -- the
 *        same fold-over-SerializableComponents pattern SceneManager itself
 *        already uses for its own save-snapshot and cross-scene copies
 *        (SceneId isn't serializable data, so that generic copy can't
 *        carry it; this tags it separately as the one extra step needed).
 */
asge::ecs::Entity DuplicateEntity(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSource, asge::str::String const& inScenePath ) noexcept
{
    auto created = inRegistry.CreateEntity();
    if ( !created ) return asge::ecs::Entity::Null();

    std::apply( [&]( auto ... component )
    {
        ( [&]
        {
            using T = decltype(component);
            if ( inRegistry.HasComponent<T>( inSource ) )
            {
                inRegistry.AddComponent<T>( created.Value(), inRegistry.GetComponent<T>( inSource ).Value().get() );
            }
        }(), ... );
    }, asge::game::components::SerializableComponents{} );

    inRegistry.AddComponent<asge::game::scene::SceneId>( created.Value(), asge::game::scene::SceneId{ inScenePath } );
    return created.Value();
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
 *        creating a new scene, never silently loses edits), evicts it from
 *        SceneManager's cache, then SceneManager::LoadSceneFromFile reads
 *        the new one from disk.
 *
 * The evict is deliberate, not just cleanup: SceneManager keeps ONE shared
 * Registry for every scene it's ever loaded, distinguishing them only by a
 * SceneId tag -- switching scenes without evicting the old one leaves both
 * resident simultaneously. RenderSystem/DrawEntityListPanel/viewport
 * picking/etc. all operate on that whole Registry with no scoping of their
 * own to "just the active scene's entities" (there's no engine concept of
 * that), so a second resident scene doesn't just sit inertly cached -- it
 * keeps rendering, keeps showing up in the Entities list, keeps being
 * pickable, right alongside whatever's actually selected in the Scene
 * dropdown. Evicting on every switch means only one scene is ever resident
 * at a time, at the cost of the disk-free instant-swap SceneManager's own
 * cache would otherwise give switching back to an already-visited scene --
 * correctness over that micro-optimization here.
 */
void SwitchToScene(
    Project& inOutProject, int inNewIndex,
    asge::game::scene::SceneManager& inSceneManager, asge::game::asset::AssetManager& inAssets,
    asge::video::IRenderer& inRenderer, asge::ecs::Entity& ioSelected ) noexcept
{
    std::optional<std::filesystem::path> previousPath;
    if ( inOutProject.m_ActiveSceneIndex >= 0
      && inOutProject.m_ActiveSceneIndex < static_cast<int>( inOutProject.m_Scenes.size() ) )
    {
        auto& current = inOutProject.m_Scenes[inOutProject.m_ActiveSceneIndex];
        if ( current.m_Dirty )
        {
            auto const saveResult = inSceneManager.SaveScene( current.m_Path );
            if ( saveResult ) current.m_Dirty = false; else saveResult.LogError();
        }
        previousPath = current.m_Path;
    }

    auto const& target = inOutProject.m_Scenes[inNewIndex];
    auto const loadResult = inSceneManager.LoadSceneFromFile( target.m_Path );
    if ( !loadResult )
    {
        loadResult.LogError();
        return;
    }

    inOutProject.m_ActiveSceneIndex = inNewIndex;

    // Evict only after the switch succeeded and only if it's actually a
    // different scene -- EvictCachedScene is a documented no-op on
    // whatever's currently active anyway, but re-selecting the same scene
    // shouldn't even attempt it.
    if ( previousPath && *previousPath != target.m_Path )
    {
        inSceneManager.EvictCachedScene( previousPath->string() );
    }
    ioSelected = asge::ecs::Entity::Null();
    ResetEntityDisplayIds();
    inAssets.ResolveAssets( inSceneManager.GetRegistry(), inRenderer );
    RegisterSceneAssets( inSceneManager.GetRegistry() );
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
    // itself and auto-saved/loaded rather than explicitly, at the
    // executable's own location (SDL_GetBasePath, independent of whatever
    // the current working directory happens to be) rather than CWD.
    fs::path const sessionFilePath = [] {
        char const* base = SDL_GetBasePath();
        return base ? fs::path( base ) / "asge.session" : fs::path( "asge.session" );
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
                }
                waypointEdit = WaypointEditState{};
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
                   && event.button.button == SDL_BUTTON_LEFT
                   && !ImGui::GetIO().WantCaptureMouse)
            {
                asge::math::Float2 const screenPos{ event.button.x, event.button.y };

                if (waypointEdit.m_Active)
                {
                    // Phase 12: a click in this mode appends a waypoint to
                    // the target entity instead of picking/dragging -- the
                    // target stays fixed for the whole mode, so this never
                    // touches selectedEntity/draggingEntity.
                    auto const worldPos = asge::video::ScreenToWorld(
                        videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(), screenPos);
                    if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(waypointEdit.m_Entity))
                    {
                        pf.Value().get().m_Waypoints.push_back(worldPos);
                        MarkActiveSceneDirty(currentProject);
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
                    if (dragAxis != GizmoAxis::Y) t.m_X += event.motion.xrel / zoom;
                    if (dragAxis != GizmoAxis::X) t.m_Y += event.motion.yrel / zoom;
                    MarkActiveSceneDirty(currentProject);
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
                if (ImGui::MenuItem("Save"))
                {
                    // A project always has a real m_FilePath the moment it
                    // exists (Create a Project auto-saves it immediately),
                    // so unlike Scene's Save this never needs a Save-As
                    // fallback.
                    auto const saveResult = SaveProject(
                        vfs, sceneManager.GetRegistry(), *currentProject, gridSpacing, targetGameWidth, targetGameHeight);
                    if (!saveResult) saveResult.LogError();
                    else LOG_INFO("Project saved to ", currentProject->m_FilePath.string());
                }
                if (ImGui::MenuItem("Save As...")) openSaveProjectAsDialog = true;
                if (!hasProject) ImGui::EndDisabled();

                ImGui::Separator();
                if (!hasActiveScene) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Save Scene"))
                {
                    auto& active = currentProject->m_Scenes[currentProject->m_ActiveSceneIndex];
                    auto const saveResult = sceneManager.SaveScene(active.m_Path);
                    if (!saveResult) saveResult.LogError();
                    else
                    {
                        active.m_Dirty = false;
                        LOG_INFO("Scene saved to ", active.m_Path.string());
                    }
                }
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
        if (DrawEntityListPanel(sceneManager.GetRegistry(), selectedEntity, freshHasProject))
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

        auto const inspectorResult = DrawInspectorPanel(
            sceneManager.GetRegistry(), selectedEntity,
            KnownTexturePaths(sceneManager.GetRegistry()),
            KnownAnimationPaths(sceneManager.GetRegistry()),
            KnownAudioPaths(sceneManager.GetRegistry()),
            waypointEdit);

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
        AssetPick const assetPick = DrawAssetBrowserPanel(sceneManager.GetRegistry(), vfs, window, freshHasProject);
        if (assetPick.m_Kind != AssetPickKind::None) selectedAsset = assetPick;
        DrawAssetInspectorPanel(selectedAsset, vfs, assets, videoSys.GetRenderer(), audioDevice);

        // Always-on panel: lists/adds VirtualFileSystem mounts, and surfaces
        // any root the current scene's assets reference but isn't mounted --
        // resolves internally right after a successful mount (see its own
        // doc comment), same "call on change, not every frame" reasoning as
        // the ComponentsChanged/asset-browser resolves above.
        DrawVfsPanel(vfs, sceneManager.GetRegistry(), assets, videoSys.GetRenderer(), window, freshHasProject);

        DrawConsolePanel(window);

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
        // visible once selection ends, not just while adding them.
        if (auto pf = sceneManager.GetRegistry().GetComponent<PathFollow>(selectedEntity))
        {
            DrawPathFollowWaypointOverlay(
                videoSys.GetRenderer(), ImGui::GetBackgroundDrawList(), pf.Value().get().m_Waypoints);
        }

        ImGui::Render();

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

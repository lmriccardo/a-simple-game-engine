#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Scene/SceneId.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <optional>

#include "Inspector.hpp"
#include "ViewportOverlay.hpp"
#include "ConsolePanel.hpp"
#include "FileDialog.hpp"
#include "AssetBrowser.hpp"
#include "AssetInspector.hpp"
#include "VfsPanel.hpp"
#include "SessionManager.hpp"

namespace
{
using asge::game::components::Transform;

constexpr SDL_DialogFileFilter kSceneFileFilters[]{ { "Scene (*.toml)", "toml" } };
constexpr SDL_DialogFileFilter kSessionFileFilters[]{ { "Session (*.asges)", "asges" } };

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

    // No scene is auto-loaded on startup -- the editor opens with an empty
    // Registry, same as File > New (see below), rather than a hardcoded test
    // fixture that would only ever exist on the machine that built it. No
    // mount is auto-created either: vfs starts with nothing bound, and stays
    // that way until the user sets one up via the Virtual File System panel
    // (or opens a scene, which resolves its own file without leaving a
    // mount behind -- see LoadSceneFromRealPath).
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
    float gridSpacing = 50.0f; // world units between grid lines; editor-configurable, see the Scene window

    // Phase 9: the target game window size DrawCameraOverlays sizes each
    // Camera entity's preview box against -- an editor display setting
    // (saved/restored via SaveSession/LoadSession's own [View] table, not
    // as scene data) since nothing in the engine's own scene/config schema
    // currently models "the target game's window size" for this to read
    // instead.
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

    // The three top-center HUD panels' combined width, from last frame --
    // ImGui can't report a window's AutoResize size before it's actually
    // drawn once, so centering the group this frame off last frame's total
    // (updated after all three are drawn below) is the standard immediate-
    // mode fix for that chicken-and-egg problem. One frame of lag on a
    // width change (e.g. the mouse-position text getting longer) is
    // imperceptible; 0 here just means frame one starts slightly off-center
    // and self-corrects on frame two.
    float hudGroupWidth = 0.0f;

    // The real disk path Save writes to once the user has actually chosen one
    // (via the native Save/Save As dialog) -- nullopt for a fresh/untitled
    // scene, which is exactly what makes Save behave like Save As until then.
    // sceneManager's own SceneId-tagging identity (RenameActiveScene) is a
    // separate, purely internal bookkeeping string -- it doesn't need to
    // match this real path, so a fresh scene gets a placeholder identity
    // before any real location is known.
    std::optional<fs::path> currentScenePath;
    FileDialogResult saveDialogResult;
    FileDialogResult openDialogResult;
    FileDialogResult saveSessionDialogResult;
    FileDialogResult openSessionDialogResult;

    sceneManager.RenameActiveScene("untitled");

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

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
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
        // are drained here, once per frame, on the main thread --
        // SceneManager/Registry/vfs are never touched from the dialog
        // callback itself.
        {
            std::string chosenPath;
            if (DrainFileDialogResult(saveDialogResult, chosenPath))
            {
                fs::path path = chosenPath;
                if (path.filename().empty())
                {
                    // Dialog closed/canceled without a real filename (some
                    // backends hand back an empty or directory-only path
                    // instead of failing outright) -- checked before the
                    // extension is appended below, since after that a blank
                    // path would otherwise silently become a bare ".toml".
                    LOG_WARNING("Save dialog returned no filename -- scene not saved");
                }
                else
                {
                    if (path.extension().empty()) path += ".toml"; // native dialogs don't all enforce the filter's extension
                    sceneManager.RenameActiveScene(path.string());
                    auto const saveResult = sceneManager.SaveScene(path);
                    if (!saveResult) saveResult.LogError();
                    else
                    {
                        currentScenePath = path;
                        LOG_INFO("Scene saved to ", path.string());
                    }
                }
            }
        }
        {
            std::string chosenPath;
            if (DrainFileDialogResult(openDialogResult, chosenPath))
            {
                if (chosenPath.empty())
                {
                    // Same defensive check as Save's -- a dialog closed
                    // without a real selection shouldn't be treated as "open
                    // whatever the empty path resolves to".
                    LOG_WARNING("Open dialog returned no file -- nothing loaded");
                }
                else
                {
                    fs::path const path = chosenPath;

                    sceneManager.UnloadScene();
                    auto const loadResult = LoadSceneFromRealPath(vfs, sceneManager, path);
                    if (!loadResult)
                    {
                        loadResult.LogError();
                    }
                    else
                    {
                        currentScenePath = path;
                        selectedEntity = asge::ecs::Entity::Null();
                        ResetEntityDisplayIds();
                        LOG_INFO("Scene loaded from ", path.string());
                        // A freshly loaded scene's Sprite/Animation/AudioSource
                        // paths need resolving; anything whose virtual root isn't
                        // mounted yet fails here and shows up as "Missing" in the
                        // VFS panel, which re-resolves once the user sets it.
                        assets.ResolveAssets(sceneManager.GetRegistry(), videoSys.GetRenderer());
                        // So the Assets panel keeps showing these independently
                        // of whether any entity still references them -- see
                        // RegisterSceneAssets's own doc comment.
                        RegisterSceneAssets(sceneManager.GetRegistry());
                    }
                }
            }
        }
        {
            std::string chosenPath;
            if (DrainFileDialogResult(saveSessionDialogResult, chosenPath))
            {
                fs::path path = chosenPath;
                if (path.filename().empty())
                {
                    // Same guard as Save Scene's -- an empty/directory-only
                    // path here would otherwise become a bare ".asges".
                    LOG_WARNING("Save Session dialog returned no filename -- session not saved");
                }
                else
                {
                    if (path.extension().empty()) path += ".asges";
                    auto const saveResult = SaveSession(
                        vfs, sceneManager.GetRegistry(), currentScenePath,
                        gridSpacing, targetGameWidth, targetGameHeight, path);
                    if (!saveResult) saveResult.LogError();
                    else LOG_INFO("Session saved to ", path.string());
                }
            }
        }
        {
            std::string chosenPath;
            if (DrainFileDialogResult(openSessionDialogResult, chosenPath))
            {
                if (chosenPath.empty())
                {
                    // Same guard as Open Scene's.
                    LOG_WARNING("Open Session dialog returned no file -- nothing loaded");
                }
                else
                {
                    fs::path const path = chosenPath;
                    auto const loadResult = LoadSession(
                        vfs, sceneManager, assets, videoSys.GetRenderer(), path, currentScenePath,
                        gridSpacing, targetGameWidth, targetGameHeight);
                    if (!loadResult)
                    {
                        loadResult.LogError();
                    }
                    else
                    {
                        selectedEntity = asge::ecs::Entity::Null();
                        ResetEntityDisplayIds();
                        LOG_INFO("Session loaded from ", path.string());
                    }
                }
            }
        }

        bool openSaveDialog = false;
        bool openOpenDialog = false;
        bool openSaveSessionDialog = false;
        bool openOpenSessionDialog = false;
        bool openGridModal = false;
        bool openGameWindowModal = false;
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))
                {
                    sceneManager.UnloadScene();
                    sceneManager.RenameActiveScene("untitled");
                    currentScenePath.reset();
                    selectedEntity = asge::ecs::Entity::Null();
                    ResetEntityDisplayIds();
                }
                if (ImGui::MenuItem("Open...")) openOpenDialog = true;
                if (ImGui::MenuItem("Save"))
                {
                    if (currentScenePath)
                    {
                        // Exactly the game's own save path: SceneManager::SaveScene
                        // -> SceneSerializer -> Serializer<Transform>, unmodified.
                        auto const saveResult = sceneManager.SaveScene(*currentScenePath);
                        if (!saveResult) saveResult.LogError();
                        else LOG_INFO("Scene saved to ", currentScenePath->string());
                    }
                    else
                    {
                        // Never saved anywhere real yet -- behave like Save As
                        // rather than silently writing under the scratch dir.
                        openSaveDialog = true;
                    }
                }
                if (ImGui::MenuItem("Save As...")) openSaveDialog = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Save Session...")) openSaveSessionDialog = true;
                if (ImGui::MenuItem("Open Session...")) openOpenSessionDialog = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Grid")) openGridModal = true;
                if (ImGui::MenuItem("Game Window")) openGameWindowModal = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // OpenPopup deferred to here (same ID-stack depth BeginPopupModal
        // itself is called at) rather than issued from inside the View menu
        // above -- calling it from inside BeginMenu/EndMenu hashes to a
        // different ID and the popup silently never opens, same gotcha
        // Save/Open's native dialogs avoid via the openXDialog bools below.
        // Drafts are (re-)seeded from the live values right here too, so
        // reopening a modal after an earlier "Close" starts from what's
        // actually live again, not a stale discarded edit.
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

        if (openSaveDialog)
        {
            auto const defaultLocation = currentScenePath
                ? DialogDefaultLocation(currentScenePath->parent_path()) : std::string{};
            SDL_ShowSaveFileDialog(
                OnFileDialogResult, &saveDialogResult, window,
                kSceneFileFilters, 1, defaultLocation.empty() ? nullptr : defaultLocation.c_str());
        }
        if (openOpenDialog)
        {
            auto const defaultLocation = currentScenePath
                ? DialogDefaultLocation(currentScenePath->parent_path()) : std::string{};
            SDL_ShowOpenFileDialog(
                OnFileDialogResult, &openDialogResult, window,
                kSceneFileFilters, 1, defaultLocation.empty() ? nullptr : defaultLocation.c_str(), false);
        }
        if (openSaveSessionDialog)
        {
            SDL_ShowSaveFileDialog(
                OnFileDialogResult, &saveSessionDialogResult, window, kSessionFileFilters, 1, nullptr);
        }
        if (openOpenSessionDialog)
        {
            SDL_ShowOpenFileDialog(
                OnFileDialogResult, &openSessionDialogResult, window, kSessionFileFilters, 1, nullptr, false);
        }

        // Right-aligned, stacked above Entities/Inspector (see Inspector.hpp's
        // kEditorPanelWidth/kEditorPanelRightMargin) instead of ImGui's
        // default cascade, which left all three overlapping near the corner.
        // Position is forced every frame (ImGuiCond_Always) since DisplaySize
        // can change (resizable editor window) and the whole right-hand
        // group must stay flush against the edge regardless -- same
        // precedent as ConsolePanel's own anchor. Size alone stays
        // user-draggable.
        float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
        ImGui::SetNextWindowPos(ImVec2(rightX, 30.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kEditorPanelWidth, 55.0f), ImGuiCond_FirstUseEver);

        ImGui::Begin("Scene");
        ImGui::Text("Scene loaded: %zu entities", sceneManager.GetRegistry().AllEntities().size());
        ImGui::End();

        // Three fixed HUDs, not regular panels -- no title bar/move/
        // collapse/close, repositioned every frame (ImGuiCond_Always, not
        // FirstUseEver) so none can be dragged away, hidden behind another
        // window, or lost the way a field inside a closable/collapsible
        // panel like Scene could be. Each is placed flush against the
        // previous one's own right edge (GetWindowPos/GetWindowSize read
        // right before each End()), and the whole row is anchored off
        // hudGroupWidth so the group as a whole sits centered rather than
        // the first panel alone.
        float const hudGroupStartX = ImGui::GetIO().DisplaySize.x * 0.5f - hudGroupWidth * 0.5f;

        ImVec2 mouseHudPos, mouseHudSize;
        {
            ImGuiWindowFlags const kMouseHudFlags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
              | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
              | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

            ImGui::SetNextWindowPos(
                ImVec2( hudGroupStartX, ImGui::GetFrameHeight() + 4.0f ), ImGuiCond_Always );
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
        constexpr float kHudGap = 8.0f;
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
            // a real button, so (unlike the two pure-display HUDs either
            // side of it) this one can't be NoInputs.
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
        if (DrawEntityListPanel(sceneManager.GetRegistry(), selectedEntity))
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
            }
            else created.LogError();
        }

        switch (DrawInspectorPanel(
            sceneManager.GetRegistry(), selectedEntity,
            KnownTexturePaths(sceneManager.GetRegistry()),
            KnownAnimationPaths(sceneManager.GetRegistry()),
            KnownAudioPaths(sceneManager.GetRegistry())))
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
        AssetPick const assetPick = DrawAssetBrowserPanel(sceneManager.GetRegistry(), vfs, window);
        if (assetPick.m_Kind != AssetPickKind::None) selectedAsset = assetPick;
        DrawAssetInspectorPanel(selectedAsset, vfs, assets, videoSys.GetRenderer());

        // Always-on panel: lists/adds VirtualFileSystem mounts, and surfaces
        // any root the current scene's assets reference but isn't mounted --
        // resolves internally right after a successful mount (see its own
        // doc comment), same "call on change, not every frame" reasoning as
        // the ComponentsChanged/asset-browser resolves above.
        DrawVfsPanel(vfs, sceneManager.GetRegistry(), assets, videoSys.GetRenderer(), window);

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

        ImGui::Render();

        videoSys.GetRenderer().Clear({ 15, 15, 20, 255 });
        asge::game::systems::RenderPipeline(
            sceneManager.GetRegistry(), videoSys.GetRenderer(), asge::time::DeltaTime());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        videoSys.GetRenderer().Present();
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    return 0;
}

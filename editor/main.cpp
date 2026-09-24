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
    auto const initResult = videoSys.Initialize("ASGE Editor", 1280, 720);
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
                    auto const saveResult = SaveSession(vfs, sceneManager.GetRegistry(), currentScenePath, path);
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
                        vfs, sceneManager, assets, videoSys.GetRenderer(), path, currentScenePath);
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
            ImGui::EndMainMenuBar();
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
        float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
        ImGui::SetNextWindowPos(ImVec2(rightX, 30.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(kEditorPanelWidth, 55.0f), ImGuiCond_FirstUseEver);

        ImGui::Begin("Scene");
        ImGui::Text("Scene loaded: %zu entities", sceneManager.GetRegistry().AllEntities().size());
        ImGui::DragFloat("Grid Spacing", &gridSpacing, 1.0f, 5.0f, 500.0f);
        ImGui::End();

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

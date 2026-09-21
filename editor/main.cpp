#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
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

#include <filesystem>

#include "Inspector.hpp"
#include "ViewportOverlay.hpp"

namespace
{
using asge::game::components::Transform;

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

    // Hardcoded test scene for now (Phase 1/2 scope): staged into a scratch
    // copy of scene_demo's fixture rather than mounting it directly, so
    // Phase 2's Save can freely overwrite it without dirtying checked-in
    // example content. Replaced once the editor grows its own asset
    // browsing/authoring (later phases).
    //
    // Only seeded once -- a saved edit must survive closing and reopening
    // the editor, not get overwritten by the pristine fixture on next launch.
    namespace fs = std::filesystem;
    fs::path const scratchAssetsDir = fs::temp_directory_path() / "asge_editor_scratch_assets";
    std::error_code copyEc;
    if (!fs::exists(scratchAssetsDir))
    {
        fs::copy(ASGE_EDITOR_TEST_SCENE_DIR, scratchAssetsDir, fs::copy_options::recursive, copyEc);
    }
    if (copyEc) LOG_ERROR("Failed to stage editor scratch assets: ", copyEc.message());

    auto const mountResult = vfs.Mount("assets", scratchAssetsDir.string());
    if (!mountResult) mountResult.LogError();

    auto const loadResult = sceneManager.LoadScene("assets/scene.toml");
    if (!loadResult) loadResult.LogError();
    assets.ResolveAssets(sceneManager.GetRegistry(), videoSys.GetRenderer());

    auto* window   = static_cast<SDL_Window*>(videoSys.GetWindow().NativeHandle());
    auto* renderer = static_cast<SDL_Renderer*>(videoSys.GetRenderer().NativeHandle());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    auto selectedEntity = asge::ecs::Entity::Null();
    float gridSpacing = 50.0f; // world units between grid lines; editor-configurable, see the Scene window

    // Phase 5: just the filename under the mounted "assets" dir -- the real
    // disk path (scratchAssetsDir / this) is built directly rather than via
    // vfs.Resolve, which only finds files that already exist and so can't
    // resolve a brand new one from File > New/Save As. sceneManager's own
    // "assets/<this>" virtual path stays in sync via RenameActiveScene, so
    // ActiveEntities()/SaveScene (SceneId-filtered) keep seeing the right
    // entities regardless of which file they end up written to.
    std::string currentSceneFilename = "scene.toml";
    char saveAsBuffer[128] = "scene.toml";

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

        bool openSaveAsPopup = false;
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))
                {
                    sceneManager.UnloadScene();
                    currentSceneFilename = "untitled.toml";
                    sceneManager.RenameActiveScene("assets/" + currentSceneFilename);
                    selectedEntity = asge::ecs::Entity::Null();
                    ResetEntityDisplayIds();
                }
                if (ImGui::MenuItem("Save"))
                {
                    // Exactly the game's own save path: SceneManager::SaveScene
                    // -> SceneSerializer -> Serializer<Transform>, unmodified.
                    auto const diskPath = scratchAssetsDir / currentSceneFilename;
                    auto const saveResult = sceneManager.SaveScene(diskPath);
                    if (!saveResult) saveResult.LogError();
                    else LOG_INFO("Scene saved to ", diskPath.string());
                }
                if (ImGui::MenuItem("Save As..."))
                {
                    std::snprintf(saveAsBuffer, sizeof(saveAsBuffer), "%s", currentSceneFilename.c_str());
                    openSaveAsPopup = true; // OpenPopup deferred to outside the menu's ID scope, see below
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // OpenPopup/BeginPopupModal must run at the same ID-stack depth to
        // find each other -- calling OpenPopup while still inside the File
        // menu (a different ID scope) would hash to a different ID than
        // this top-level BeginPopupModal, and the popup would never open.
        if (openSaveAsPopup) ImGui::OpenPopup("Save As");

        if (ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Filename", saveAsBuffer, sizeof(saveAsBuffer));
            if (ImGui::Button("Save"))
            {
                currentSceneFilename = saveAsBuffer;
                sceneManager.RenameActiveScene("assets/" + currentSceneFilename);

                auto const diskPath = scratchAssetsDir / currentSceneFilename;
                auto const saveResult = sceneManager.SaveScene(diskPath);
                if (!saveResult) saveResult.LogError();
                else LOG_INFO("Scene saved to ", diskPath.string());

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
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

        switch (DrawInspectorPanel(sceneManager.GetRegistry(), selectedEntity))
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
        case EntityAction::None:
            break;
        }

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

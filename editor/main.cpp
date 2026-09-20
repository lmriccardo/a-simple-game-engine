#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>

#include <filesystem>

namespace
{
using asge::game::components::Sprite;
using asge::game::components::SpriteGetDstRect;
using asge::game::components::Transform;

/**
 * @brief Finds the topmost entity (by iteration order) whose Transform +
 *        optional Sprite bounds contain inWorldPos, or Entity::Null() if none.
 *
 * Falls back to a 1x1 point at the Transform's position for entities with no
 * Sprite (or an unresolved one) to hit-test against, per the roadmap's
 * "+ sprite bounds if available" -- no multi-select yet, so the last match
 * wins rather than resolving overlap by draw order.
 */
asge::ecs::Entity PickEntityAt( asge::ecs::Registry& inRegistry, asge::math::Float2 inWorldPos ) noexcept
{
    auto picked = asge::ecs::Entity::Null();
    for ( auto entity : inRegistry.AllEntities() )
    {
        auto transformResult = inRegistry.GetComponent<Transform>( entity );
        if ( !transformResult ) continue;
        auto const& t = transformResult.Value().get();

        asge::math::Rect hitRect{ t.m_X, t.m_Y, 1.0f, 1.0f };
        if ( auto spriteResult = inRegistry.GetComponent<Sprite>( entity ) )
        {
            if ( auto dst = SpriteGetDstRect( spriteResult.Value().get(), t ) ) hitRect = *dst;
        }

        bool const hit = inWorldPos.x() >= hitRect.m_X && inWorldPos.x() <= hitRect.m_X + hitRect.m_Width
                       && inWorldPos.y() >= hitRect.m_Y && inWorldPos.y() <= hitRect.m_Y + hitRect.m_Height;
        if ( hit ) picked = entity;
    }
    return picked;
}
}

// Phase 2: prove select -> edit -> save -> reload round-trips through the
// engine's real Transform serialization, before building any generalized
// inspector machinery. Still no Game/IGameState dependency -- picking uses
// raw SDL mouse events rather than InputState/InputSystem, which exist to
// feed IGameState::Update()'s polling model the editor doesn't have.
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
                // Screen->world via the renderer's own camera/viewport math
                // (asge::video::ScreenToWorld), not a re-derived inverse.
                asge::math::Float2 const screenPos{ event.button.x, event.button.y };
                auto const worldPos = asge::video::ScreenToWorld(
                    videoSys.GetRenderer().GetCamera(), videoSys.GetRenderer().GetViewport(), screenPos);
                selectedEntity = PickEntityAt(sceneManager.GetRegistry(), worldPos);
            }
        }

        GET_TIMESYSTEM.Tick();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save"))
                {
                    // Exactly the game's own save path: SceneManager::SaveScene
                    // -> SceneSerializer -> Serializer<Transform>, unmodified.
                    auto const resolved = vfs.Resolve("assets/scene.toml");
                    if (resolved)
                    {
                        auto const saveResult = sceneManager.SaveScene(resolved.Value());
                        if (!saveResult) saveResult.LogError();
                    }
                    else resolved.LogError();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGui::Begin("Scene");
        ImGui::Text("Scene loaded: %zu entities", sceneManager.GetRegistry().AllEntities().size());
        ImGui::End();

        if (selectedEntity != asge::ecs::Entity::Null())
        {
            if (auto transformResult = sceneManager.GetRegistry().GetComponent<Transform>(selectedEntity))
            {
                auto& t = transformResult.Value().get();
                ImGui::Begin("Inspector");
                ImGui::Text("Entity #%u", selectedEntity.m_Index);
                float pos[2]{ t.m_X, t.m_Y };
                if (ImGui::DragFloat2("Position", pos))
                {
                    t.m_X = pos[0];
                    t.m_Y = pos[1];
                }
                ImGui::End();
            }
        }

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

#include <ASGE/Video/VideoSystem.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Game/Systems/RenderSystem.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>

// Phase 1: prove the editor operates on genuine engine state -- load a real
// authored .toml scene through the same SceneManager/AssetManager path the
// game uses, and render it through the same RenderPipeline, rather than a
// mock. Still uses VideoSystem directly (not asge::Application<TGame>), for
// the same reason as Phase 0: no Game/IGameState dependency.
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

    // Hardcoded test scene for now (Phase 1 scope) -- reuses scene_demo's
    // fixture rather than duplicating it; replaced once the editor grows
    // its own asset browsing/authoring (later phases).
    auto const mountResult = vfs.Mount("assets", ASGE_EDITOR_TEST_SCENE_DIR);
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
        }

        GET_TIMESYSTEM.Tick();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Scene");
        ImGui::Text("Scene loaded: %zu entities", sceneManager.GetRegistry().AllEntities().size());
        ImGui::End();

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

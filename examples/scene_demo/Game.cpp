#include "Game.hpp"

#include <algorithm>
#include <filesystem>

namespace
{
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::Velocity;

constexpr float kWindowWidth  = 800.0f;
constexpr float kWindowHeight = 600.0f;
constexpr float kPlayerSpeed  = 220.0f;
}

SceneDemoGame::SceneDemoGame()
{
    // ASGE_SCENE_DEMO_ASSET_DIR is injected by CMakeLists.txt -- mounted
    // once so both the scene file and every Sprite's texture inside it are
    // resolved by virtual path, not a hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("assets", ASGE_SCENE_DEMO_ASSET_DIR);
    if ( !mountResult ) { mountResult.LogError(); return; }

    auto loadResult = m_SceneSerializer.Load(m_Registry, "assets/scene.toml");
    if ( !loadResult ) { loadResult.LogError(); return; }

    // The scene's last entity is the player, by convention -- see
    // scene.toml's own comments for why (no entity name/tag/id concept
    // exists yet to do this properly).
    auto entities = m_Registry.AllEntities();
    if ( !entities.empty() ) m_Player = entities.back();
}

void SceneDemoGame::ResolveSpriteTextures(asge::video::IRenderer &inRenderer)
{
    for ( auto [ entity, sprite ] : m_Registry.View<Sprite>() )
    {
        (void)entity;
        Sprite& s = sprite.get();
        if ( s.m_Texture || s.m_VirtualPath.empty() ) continue;

        // Cached by virtual path so entities sharing one texture (every
        // sprite in this demo, all pointing at assets/checker.bmp) only
        // pay for CreateTexture once.
        auto cached = m_Textures.find(s.m_VirtualPath);
        if ( cached == m_Textures.end() )
        {
            auto imageAsset = m_Assets.GetImage(s.m_VirtualPath);
            if ( !imageAsset )
            {
                imageAsset.LogError();
                continue;
            }

            auto texture = inRenderer.CreateTexture(imageAsset.Value()->Get());
            if ( !texture ) continue;
            cached = m_Textures.emplace(s.m_VirtualPath, std::move(texture)).first;
        }

        s.m_Texture = cached->second.get();
    }
}

void SceneDemoGame::WrapAroundScreen()
{
    // Demo-specific dressing (not part of the shared Game/Systems library):
    // keeps drifting entities on screen by teleporting them across once
    // they fully exit one edge. Same approach as ecs_demo.
    for ( auto [ entity, transform ] : m_Registry.View<Transform>() )
    {
        (void)entity;
        auto& t = transform.get();
        float const margin = 64.0f * std::max(t.m_ScaleX, t.m_ScaleY);

        if ( t.m_X < -margin )                    t.m_X = kWindowWidth + margin;
        else if ( t.m_X > kWindowWidth + margin )  t.m_X = -margin;

        if ( t.m_Y < -margin )                     t.m_Y = kWindowHeight + margin;
        else if ( t.m_Y > kWindowHeight + margin )  t.m_Y = -margin;
    }
}

void SceneDemoGame::UpdatePlayerVelocity(asge::input::InputState const& inInput)
{
    if ( m_Player == asge::ecs::Entity::Null() ) return;

    auto result = m_Registry.GetComponent<Velocity>(m_Player);
    if ( !result ) return;

    // Continuous, held-down movement -- IsKeyDown polling, same as
    // input_demo's UpdatePlayerBox, instead of hand-tracking bools from
    // OnSystemEvent's press/release events.
    Velocity& velocity = result.Value().get();
    velocity.m_DX = (inInput.IsKeyDown(asge::input::Keycode::D) ? kPlayerSpeed : 0.0f)
                  - (inInput.IsKeyDown(asge::input::Keycode::A) ? kPlayerSpeed : 0.0f);
    velocity.m_DY = (inInput.IsKeyDown(asge::input::Keycode::S) ? kPlayerSpeed : 0.0f)
                  - (inInput.IsKeyDown(asge::input::Keycode::W) ? kPlayerSpeed : 0.0f);
}

void SceneDemoGame::SaveSceneSnapshot() const
{
    // Written next to the temp dir, not back over the checked-in
    // assets/scene.toml -- this demo's point is that the *live*, moved
    // around Registry round-trips, not that it should overwrite its own
    // source asset every time someone presses P.
    auto const path = std::filesystem::temp_directory_path() / "asge_scene_demo_saved.toml";
    auto result = m_SceneSerializer.Save(m_Registry, path);
    if ( !result ) { result.LogError(); return; }
    LOG_INFO("Scene snapshot saved to ", path.string());
}

void SceneDemoGame::Update(float inDeltaTime, asge::input::InputState const& inInput)
{
    UpdatePlayerVelocity(inInput);
    asge::game::systems::MovementSystem(m_Registry, inDeltaTime);
    WrapAroundScreen();

    // Edge-triggered -- IsKeyPressed, not IsKeyDown, so one tap saves once
    // instead of once per frame the key happens to be held.
    if ( inInput.IsKeyPressed(asge::input::Keycode::P) ) SaveSceneSnapshot();
}

void SceneDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 15, 15, 20, 255 });

    if ( !m_TexturesResolved )
    {
        ResolveSpriteTextures(inRenderer);
        m_TexturesResolved = true;
    }

    asge::game::systems::RenderSystem(m_Registry, inRenderer);
}

void SceneDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Deliberately empty -- every reaction to input in this example comes
    // from polling InputState in Update() instead (see input_demo).
}

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

SceneDemoState::SceneDemoState(
    asge::game::scene::SceneManager& inSceneManager, asge::game::asset::AssetManager& inAssets)
: m_SceneManager(inSceneManager), m_Assets(inAssets)
{
    RefreshPlayerReference();
}

void SceneDemoState::RefreshPlayerReference()
{
    // The active scene's last entity is the player, by convention -- see
    // scene.toml's own comments for why (no entity name/tag/id concept
    // exists yet to do this properly). ActiveEntities(), not GetRegistry()
    // -- the shared Registry also holds any other resident-but-inactive
    // scene's entities, whose "last one" isn't this scene's player at all.
    // Re-run after every (re)load, since a fresh load hands out entirely
    // new Entity handles (a residency hit reuses the same ones, but
    // there's no harm re-deriving m_Player from them either way).
    auto active = m_SceneManager.ActiveEntities();
    m_Player = active.empty() ? asge::ecs::Entity::Null() : active.back();
}

void SceneDemoState::WrapAroundScreen()
{
    // Demo-specific dressing (not part of the shared Game/Systems library):
    // keeps drifting entities on screen by teleporting them across once
    // they fully exit one edge. Same approach as ecs_demo.
    auto& registry = m_SceneManager.GetRegistry();
    for ( auto entity : m_SceneManager.ActiveEntities() )
    {
        auto transform = registry.GetComponent<Transform>(entity);
        if ( !transform ) continue;

        auto& t = transform.Value().get();
        float const margin = 64.0f * std::max(t.m_LocalScale.x(), t.m_LocalScale.y());

        if ( t.m_LocalCoordinates.x() < -margin )                    { t.m_LocalCoordinates.x() = kWindowWidth + margin; t.m_Dirty = true; }
        else if ( t.m_LocalCoordinates.x() > kWindowWidth + margin )  { t.m_LocalCoordinates.x() = -margin; t.m_Dirty = true; }

        if ( t.m_LocalCoordinates.y() < -margin )                     { t.m_LocalCoordinates.y() = kWindowHeight + margin; t.m_Dirty = true; }
        else if ( t.m_LocalCoordinates.y() > kWindowHeight + margin )  { t.m_LocalCoordinates.y() = -margin; t.m_Dirty = true; }
    }
}

void SceneDemoState::UpdatePlayerVelocity(asge::input::InputState const& inInput)
{
    if ( m_Player == asge::ecs::Entity::Null() ) return;

    auto result = m_SceneManager.GetRegistry().GetComponent<Velocity>(m_Player);
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

void SceneDemoState::SaveSceneSnapshot() const
{
    // Written next to the temp dir, not back over the checked-in
    // assets/scene*.toml -- this demo's point is that the *live*, moved
    // around Registry round-trips, not that it should overwrite its own
    // source asset every time someone presses P. SaveScene() only writes
    // the active scene's entities, not any other resident-but-inactive one.
    auto const path = std::filesystem::temp_directory_path() / "asge_scene_demo_saved.toml";
    auto result = m_SceneManager.SaveScene(path);
    if ( !result ) { result.LogError(); return; }
    LOG_INFO("Scene snapshot saved to ", path.string());
}

std::optional<asge::game::state::Transition<int>>
SceneDemoState::Update([[maybe_unused]] float inDeltaTime, asge::input::InputState const& inInput)
{
    UpdatePlayerVelocity(inInput);
    WrapAroundScreen();

    // Edge-triggered -- IsKeyPressed, not IsKeyDown, so one tap saves once
    // instead of once per frame the key happens to be held.
    if ( inInput.IsKeyPressed(asge::input::Keycode::P) ) SaveSceneSnapshot();

    // L requests a swap to the second scene file, alternating back and
    // forth on repeated presses. RequestLoad() only *queues* it -- the
    // swap itself happens below, via ApplyPendingTransition(), now that
    // WrapAroundScreen is done iterating this frame's active entities. Doing the swap immediately from inside this
    // Update() would risk mutating the very entity list those two just
    // iterated.
    if ( inInput.IsKeyPressed(asge::input::Keycode::L) )
    {
        auto const& currentPath = m_SceneManager.CurrentScenePath();
        bool const onAlt = currentPath.has_value() && *currentPath == "assets/scene_alt.toml";
        m_SceneManager.RequestLoad( onAlt ? "assets/scene.toml" : "assets/scene_alt.toml" );
    }

    if ( m_SceneManager.HasPendingTransition() )
    {
        auto result = m_SceneManager.ApplyPendingTransition();
        if ( !result ) { result.LogError(); return std::nullopt; }
        RefreshPlayerReference(); // the new active scene's "last entity" is a different one
    }

    return std::nullopt;
}

void SceneDemoState::Render([[maybe_unused]] asge::video::IRenderer &inRenderer)
{
}

void SceneDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Deliberately empty -- every reaction to input in this example comes
    // from polling InputState in Update() instead (see input_demo).
}

SceneDemoGame::SceneDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    // ASGE_SCENE_DEMO_ASSET_DIR is injected by CMakeLists.txt -- mounted
    // once so both scene files and every Sprite's texture inside them are
    // resolved by virtual path, not a hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("assets", ASGE_SCENE_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    auto loadResult = m_SceneManager.LoadScene("assets/scene.toml");
    if ( !loadResult ) loadResult.LogError();

    SetInitialState(0);
}

std::unique_ptr<SceneDemoGame::StateType> SceneDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<SceneDemoState>( m_SceneManager, m_Assets );
}

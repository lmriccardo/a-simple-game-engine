#include "Game.hpp"

#include <ASGE/Game/Systems/AnimatorSystem.hpp>

namespace
{
using asge::game::components::Animator;
using asge::game::components::Sprite;
using asge::game::components::Transform;

constexpr float kSpriteScale = 4.0f; // draws each 32x32 cell at 128x128

constexpr char const* kSheetPath   = "anim/spritesheet.bmp";
constexpr char const* kClipSetPath = "anim/player.animset";
}

AnimatorDemoState::AnimatorDemoState(
    asge::ecs::Registry& inRegistry, asge::game::asset::AssetManager& inAssets)
: m_Registry(inRegistry), m_Assets(inAssets)
{
    Spawn( asge::math::Float2{ 200.0f, 236.0f }, 1.0f );
    Spawn( asge::math::Float2{ 472.0f, 236.0f }, 0.5f );
}

void AnimatorDemoState::Spawn( asge::math::Float2 inPosition, float inPlaybackSpeed )
{
    auto entity = m_Registry.CreateEntity();
    if ( !entity ) { entity.LogError(); return; }

    m_Registry.AddComponent<Transform>( entity.Value(), Transform{
        .m_LocalCoordinates = inPosition, .m_LocalScale = {kSpriteScale, kSpriteScale},
        .m_WorldCoordinates = inPosition, .m_WorldScale = {kSpriteScale, kSpriteScale}
    } );

    // The texture and the clip set stay unresolved until
    // AssetManager::ResolveAssets runs before rendering.
    m_Registry.AddComponent<Sprite>( entity.Value(), Sprite{ .m_VirtualPath = kSheetPath } );
    m_Registry.AddComponent<Animator>( entity.Value(), Animator{
        .m_ClipSetPath = kClipSetPath, .m_PlaybackSpeed = inPlaybackSpeed
    } );

    m_Entities.push_back( entity.Value() );
}

void AnimatorDemoState::HandleInput( asge::input::InputState const& inInput )
{
    using asge::input::Keycode;
    namespace comp = asge::game::components;

    bool const running = inInput.IsKeyDown( Keycode::A ) || inInput.IsKeyDown( Keycode::D );
    bool const attack  = inInput.IsKeyPressed( Keycode::J );
    bool const hurt    = inInput.IsKeyPressed( Keycode::K );
    bool const toggle  = inInput.IsKeyPressed( Keycode::SPACE );
    if ( toggle ) m_Stopped = !m_Stopped;

    for ( auto entity : m_Entities )
    {
        auto result = m_Registry.GetComponent<Animator>( entity );
        if ( !result ) continue;
        auto& animator = result.Value().get();

        // SetFlag/Fire return false until the clip set is resolved
        comp::SetFlag( animator, "running", running );
        if ( attack ) comp::Fire( animator, "attack" );
        if ( hurt )   comp::Fire( animator, "hurt" );

        if ( toggle )
        {
            if ( m_Stopped ) comp::Stop( animator );
            else             comp::Resume( animator );
        }
    }
}

std::optional<asge::game::state::Transition<int>>
AnimatorDemoState::Update( float inDeltaTime, asge::input::InputState const& inInput )
{
    HandleInput( inInput );
    asge::game::systems::AnimatorSystem( m_Registry, inDeltaTime );
    return std::nullopt;
}

void AnimatorDemoState::Render([[maybe_unused]] asge::video::IRenderer& inRenderer)
{
}

void AnimatorDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const& inSysEvent)
{
}

AnimatorDemoGame::AnimatorDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    auto mountResult = m_Vfs.Mount("anim", ASGE_ANIMATOR_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    SetInitialState(0);
}

std::unique_ptr<AnimatorDemoGame::StateType> AnimatorDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<AnimatorDemoState>( m_SceneManager.GetRegistry(), m_Assets );
}

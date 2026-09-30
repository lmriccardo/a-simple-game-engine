#pragma once

#include <memory>
#include <unordered_map>
#include <type_traits>

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Events/Events.hpp>
#include <ASGE/Input/InputState.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

#include "Assets/AssetManager.hpp"
#include "Project/Project.hpp"
#include "Scene/SceneManager.hpp"
#include "States/GameStateStack.hpp"
#include "Systems/AudioSystem.hpp"
#include "Systems/PhysicsSystem.hpp"
#include "Systems/RenderSystem.hpp"
#include "Systems/TransformPropagationSystem.hpp"
#include "Systems/UISystem.hpp"

namespace asge::game
{

namespace details
{

/** @brief The non-template surface Application drives: per-frame update/render plus system events. */
class IGame
{
public:
    virtual ~IGame() = default;

    /** @brief Advances the game by one frame. */
    virtual void Update(float inDeltaTime, input::InputState const& inInput) = 0;

    /** @brief Draws the current frame. */
    virtual void Render(video::IRenderer& inRenderer) = 0;

    /** @brief Called for every system event Application's loop pumps. */
    virtual void OnSystemEvent(event::SystemEvent const& inSysEvent) = 0;

    /** @brief True once a state has requested a clean shutdown via TransitionKind::Quit. */
    [[nodiscard]] virtual bool QuitRequested() const noexcept = 0;
};

}

/**
 * @brief Base for a concrete game: owns the asset/scene/VFS stack and a
 *        state::GameStateStack<TStateId>, and wires state Update() requests
 *        into stack transitions.
 *
 * A derived class implements CreateState() (one case per TStateId) and calls
 * SetInitialState() from its own constructor to seed the stack. States are
 * cached by id in m_StateCache the first time each is needed — CreateState()
 * only runs once per id unless InvalidateState() evicts it — so Push/Pop
 * between the same two states doesn't reconstruct them each time.
 */
template<typename TStateId>
class Game : public details::IGame
{
public:
    using StateType  = state::IGameState<TStateId>;
    using Transition = state::Transition<TStateId>;

protected:
    filesystem::VirtualFileSystem m_Vfs;
    asset::AssetManager           m_Assets{ m_Vfs };
    scene::SceneManager           m_SceneManager{ m_Vfs };
    video::IRenderer&             m_Renderer;
    audio::AudioDevice&           m_AudioDev;
    project::ProjectData          m_Project;

private:
    state::GameStateStack<TStateId>                           m_States;
    std::unordered_map<TStateId, std::unique_ptr<StateType>>  m_StateCache;
    bool                                                      m_QuitRequested{false};
    systems::PhysicsState                                     m_PhysicsState;
    float                                                     m_LastDeltaTime{0.0f};

    /** @brief Returns inId's cached state, creating it via CreateState() on first use. */
    StateType& GetOrCreateState( TStateId inId )
    {
        auto it = m_StateCache.find( inId );
        if ( it == m_StateCache.end() )
        {
            it = m_StateCache.emplace( inId, CreateState( inId ) ).first;
        }
        return *it->second;
    }

    /**
     * @brief Applies one state's requested Push/Pop/Replace to m_States, or
     *        flags a Quit for QuitRequested(); TransitionKind::None is a no-op.
     */
    void ApplyTransition( Transition const& inTransition )
    {
        switch ( inTransition.m_Kind )
        {
        case state::TransitionKind::Pop: m_States.PopRaw(); return;
        case state::TransitionKind::Push:
            m_States.PushRaw( &GetOrCreateState( inTransition.m_TargetId ) ); return;

        case state::TransitionKind::Replace:
            m_States.ReplaceRaw( &GetOrCreateState( inTransition.m_TargetId ) ); return;

        case state::TransitionKind::Quit: m_QuitRequested = true; return;

        default: return;
        }
    }

protected:
    /** @brief Constructs the state for inId; called at most once per id unless InvalidateState() runs. */
    virtual std::unique_ptr<StateType> CreateState( TStateId inId ) = 0;

    /** @brief Seeds the stack with inId as the bottom (and initially only) state. */
    void SetInitialState( TStateId inId ) noexcept
    {
        m_States.PushRaw( &GetOrCreateState( inId ) );
    }

    /** @brief Evicts inId's cached state, so the next transition to it calls CreateState() again. */
    void InvalidateState( TStateId inId ) noexcept
    {
        m_StateCache.erase( inId );
    }

public:
    explicit Game( video::IRenderer& inRenderer, audio::AudioDevice& inAudioDev ) noexcept
    : m_Renderer( inRenderer ), m_AudioDev( inAudioDev )
    {}

    /** @brief Loads inPath as the active scene and resolves its Sprite/Animation assets through m_Renderer. */
    BoolResult LoadScene( str::String const& inPath ) noexcept
    {
        auto result = m_SceneManager.LoadScene( inPath );
        if ( !result ) return result;
        m_Assets.ResolveAssets( m_SceneManager.GetRegistry(), m_Renderer );
        return result;
    }

    /**
     * @brief Loads a `.asgeproject` written by the editor: mounts its VFS entries (skipping
     *        missing directories with a warning) and loads its main scene (the first listed one
     *        if none is set) as the active one. Fails with SceneError::EmptyProject if the
     *        project lists no scenes.
     */
    BoolResult LoadProject( filesystem::Path const& inPath ) noexcept
    {
        auto project = project::LoadProjectFile( inPath );
        if ( !project ) return BoolResult::Err( project.Error() );

        for ( auto const& mount : project.Value().m_Mounts )
        {
            if ( !std::filesystem::exists( mount.m_RealDirectory ) )
            {
                LOG_WARNING( "Project mount \"", mount.m_Name, "\" -> \"", mount.m_RealDirectory.string(), "\" does not exist, skipping" );
                continue;
            }

            if ( auto result = m_Vfs.Mount( mount.m_Name, mount.m_RealDirectory.string() ); !result ) result.LogError();
        }

        m_Project = project.Value();
        if ( m_Project.m_Scenes.empty() ) return BoolResult::Err( make_error_code( errors::SceneError::EmptyProject ) );

        auto const& startScene = m_Project.m_MainScene.empty() ? m_Project.m_Scenes.front() : m_Project.m_MainScene;
        auto result = m_SceneManager.LoadSceneFromFile( startScene );
        if ( !result ) return result;
        m_Assets.ResolveAssets( m_SceneManager.GetRegistry(), m_Renderer );
        return result;
    }

    /** @brief The project last passed to LoadProject(); empty before that. */
    [[nodiscard]] project::ProjectData const& GetProject() const noexcept { return m_Project; }

    /**
     * @brief Runs one frame over the active scene's registry in the only valid order:
     *        UI interaction, the states' own Update() (game logic), gravity, movement,
     *        path following, UI layout, one transform propagation, collisions, triggers,
     *        audio. States never call these systems themselves.
     */
    void Update( float inDeltaTime, input::InputState const& inInput ) override
    {
        m_LastDeltaTime = inDeltaTime;
        systems::UIInteractionSystem( m_SceneManager.GetRegistry(), inInput, video::Camera{} );

        if ( auto transition = m_States.Update( inDeltaTime, inInput ) )
            ApplyTransition( *transition );

        auto& registry = m_SceneManager.GetRegistry();
        systems::GravitySystem( registry, inDeltaTime );
        systems::MovementSystem( registry, inDeltaTime );
        systems::PathFollowingSystem( registry, inDeltaTime );
        systems::UILayoutSystem( registry );
        systems::TransformPropagationSystem( registry );

        auto contacts = systems::DetectCollisions( registry );
        systems::ResolveCollisions( registry, contacts );
        systems::DispatchTriggerEvents( m_PhysicsState, contacts );

        systems::AudioSystem( registry, m_AudioDev );
    }

    /**
     * @brief Resolves pending assets, clears with the visible state's ClearColor(), lets states
     *        draw RenderBackground(), draws the scene through RenderPipeline, then states' Render().
     */
    void Render( video::IRenderer& inRenderer ) override
    {
        auto& registry = m_SceneManager.GetRegistry();
        m_Assets.ResolveAssets( registry, inRenderer );
        inRenderer.Clear( m_States.ClearColor() );
        m_States.RenderBackground( inRenderer );
        systems::RenderPipeline( registry, inRenderer, m_LastDeltaTime );
        m_States.Render( inRenderer );
    }

    void OnSystemEvent( event::SystemEvent const& inSysEvent ) override
    {
        m_States.OnSystemEvent( inSysEvent );
    }

    [[nodiscard]] bool QuitRequested() const noexcept override { return m_QuitRequested; }
};

/** @brief Detects (via derived-to-base conversion) whether TDerived derives from some Game<T>. */
template<typename T>
std::true_type is_game_helper( Game<T> const volatile& );
std::false_type is_game_helper( ... );

/** @brief True (as a type) if TDerived publicly derives from some Game<T> — see is_game_v. */
template<typename TDerived>
using is_game = decltype( is_game_helper( std::declval<TDerived&>() ) );

/** @brief True if TDerived publicly derives from some Game<T> — the constraint Application<TGame> requires. */
template<typename TDerived>
inline constexpr bool is_game_v = is_game<TDerived>::value;

}

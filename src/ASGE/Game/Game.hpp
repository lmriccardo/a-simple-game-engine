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
#include "Resources/HitEntry.hpp"
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

/**
 * @brief The non-template surface Application drives.
 *
 * Per-frame update/render plus system events.
 */
class IGame
{
public:
    /**
     * @brief Virtual so a game can be destroyed through the interface.
     */
    virtual ~IGame() = default;

    /**
     * @brief Advances the game by one frame.
     *
     * Called by Application once per loop iteration with the frame's delta
     * time in seconds and the current input snapshot.
     */
    virtual void Update(float inDeltaTime, input::InputState const& inInput) = 0;

    /**
     * @brief Draws the current frame.
     *
     * Called by Application after Update(), with the renderer to draw into.
     */
    virtual void Render(video::IRenderer& inRenderer) = 0;

    /**
     * @brief Called for every system event Application's loop pumps.
     *
     * Window, keyboard and mouse events arrive here before the next Update().
     */
    virtual void OnSystemEvent(event::SystemEvent const& inSysEvent) = 0;

    /**
     * @brief True once a state has requested a clean shutdown via TransitionKind::Quit.
     *
     * Application polls it to end its loop.
     */
    [[nodiscard]] virtual bool QuitRequested() const noexcept = 0;
};

}

/**
 * @brief Base for a concrete game.
 *
 * Owns the asset/scene/VFS stack and a state::GameStateStack<TStateId>, and
 * wires state Update() requests into stack transitions.
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
    filesystem::VirtualFileSystem m_Vfs;                  // Virtual mounts assets and scenes resolve through
    asset::AssetManager           m_Assets{ m_Vfs };      // Loads and caches assets
    scene::SceneManager           m_SceneManager{ m_Vfs };// The active scene and its registry
    video::IRenderer&             m_Renderer;             // Renderer assets are resolved against; not owned
    audio::AudioDevice&           m_AudioDev;             // Audio device AudioSystem plays on; not owned
    project::ProjectData          m_Project;              // The project last loaded by LoadProject()

private:
    state::GameStateStack<TStateId>                           m_States;           // The live state stack
    std::unordered_map<TStateId, std::unique_ptr<StateType>>  m_StateCache;       // Owns every state created so far
    bool                                                      m_QuitRequested{false}; // Set by a Quit transition
    systems::PhysicsState                                     m_PhysicsState;     // Trigger bookkeeping kept across frames
    float                                                     m_LastDeltaTime{0.0f}; // Delta time of the last Update(), for Render()

    /**
     * @brief Returns inId's cached state, creating it via CreateState() on first use.
     *
     * The returned reference stays valid until InvalidateState() evicts it.
     */
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
     * @brief Applies one state's requested transition.
     *
     * Push/Pop/Replace change m_States, Quit flags QuitRequested(), and
     * TransitionKind::None is a no-op.
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
    /**
     * @brief Constructs the state for inId.
     *
     * Called at most once per id unless InvalidateState() runs.
     */
    virtual std::unique_ptr<StateType> CreateState( TStateId inId ) = 0;

    /**
     * @brief Seeds the stack with inId as the bottom (and initially only) state.
     *
     * Call it from the derived constructor, once CreateState() can be used.
     */
    void SetInitialState( TStateId inId ) noexcept
    {
        m_States.PushRaw( &GetOrCreateState( inId ) );
    }

    /**
     * @brief Evicts inId's cached state, so the next transition to it calls CreateState() again.
     *
     * Do not evict a state that is currently on the stack.
     */
    void InvalidateState( TStateId inId ) noexcept
    {
        m_StateCache.erase( inId );
    }

public:
    /**
     * @brief Creates a game that draws with inRenderer and plays on inAudioDev.
     *
     * Both are held by reference and must outlive the game. Also sets an
     * empty UIHitList resource on the scene registry, which opts the scene
     * into UI hover and click detection.
     */
    explicit Game( video::IRenderer& inRenderer, audio::AudioDevice& inAudioDev ) noexcept
    : m_Renderer( inRenderer ), m_AudioDev( inAudioDev )
    {
        m_SceneManager.GetRegistry().SetResource( resources::UIHitList{} ); // opts the scene into UI hover/click detection
    }

    /**
     * @brief Loads inPath as the active scene and resolves its assets through m_Renderer.
     *
     * inPath is a virtual path. Sprite, animation, audio and font assets are
     * resolved right away; a failed scene load is returned and nothing is
     * resolved.
     */
    BoolResult LoadScene( str::String const& inPath ) noexcept
    {
        auto result = m_SceneManager.LoadScene( inPath );
        if ( !result ) return result;
        m_Assets.ResolveAssets( m_SceneManager.GetRegistry(), m_Renderer );
        return result;
    }

    /**
     * @brief Loads a `.asgeproject` written by the editor.
     *
     * Mounts its VFS entries (skipping missing directories with a warning)
     * and loads its main scene (the first listed one if none is set) as the
     * active one. Fails with SceneError::EmptyProject if the project lists no
     * scenes.
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

    /**
     * @brief The project last passed to LoadProject().
     *
     * Empty before that.
     */
    [[nodiscard]] project::ProjectData const& GetProject() const noexcept { return m_Project; }

    /**
     * @brief Runs one frame over the active scene's registry in the only valid order.
     *
     * UI interaction, the states' own Update() (game logic), gravity,
     * movement, path following, UI layout, one transform propagation,
     * collisions, triggers, audio. States never call these systems
     * themselves.
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
        systems::CollisionSystem( registry, m_PhysicsState );
        systems::AudioSystem( registry, m_AudioDev );
    }

    /**
     * @brief Draws one frame.
     *
     * Resolves pending assets, clears with the visible state's ClearColor(),
     * lets states draw RenderBackground(), draws the scene through
     * RenderPipeline, then calls the states' Render().
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

    /**
     * @brief Forwards the event to the topmost state.
     */
    void OnSystemEvent( event::SystemEvent const& inSysEvent ) override
    {
        m_States.OnSystemEvent( inSysEvent );
    }

    /**
     * @brief True once a state has requested a Quit transition.
     */
    [[nodiscard]] bool QuitRequested() const noexcept override { return m_QuitRequested; }
};

/**
 * @brief Detects (via derived-to-base conversion) whether a type derives from some Game<T>.
 *
 * Declared only, for use in an unevaluated context by is_game.
 */
template<typename T>
std::true_type is_game_helper( Game<T> const volatile& );
std::false_type is_game_helper( ... );

/**
 * @brief True (as a type) if TDerived publicly derives from some Game<T>.
 *
 * See is_game_v.
 */
template<typename TDerived>
using is_game = decltype( is_game_helper( std::declval<TDerived&>() ) );

/**
 * @brief True if TDerived publicly derives from some Game<T>.
 *
 * The constraint Application<TGame> requires.
 */
template<typename TDerived>
inline constexpr bool is_game_v = is_game<TDerived>::value;

}

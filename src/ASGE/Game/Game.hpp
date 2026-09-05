#pragma once

#include <memory>
#include <unordered_map>
#include <type_traits>

#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Events/Events.hpp>
#include <ASGE/Input/InputState.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

#include "Assets/AssetManager.hpp"
#include "Scene/SceneManager.hpp"
#include "States/GameStateStack.hpp"

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

private:
    state::GameStateStack<TStateId>                          m_States;
    std::unordered_map<TStateId, std::unique_ptr<StateType>> m_StateCache;

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

    /** @brief Applies one state's requested Push/Pop/Replace to m_States; TransitionKind::None is a no-op. */
    void ApplyTransition( Transition const& inTransition )
    {
        switch ( inTransition.m_Kind )
        {
        case state::TransitionKind::Pop: m_States.PopRaw(); return;
        case state::TransitionKind::Push:
            m_States.PushRaw( &GetOrCreateState( inTransition.m_TargetId ) ); return;

        case state::TransitionKind::Replace:
            m_States.ReplaceRaw( &GetOrCreateState( inTransition.m_TargetId ) ); return;

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
    explicit Game( video::IRenderer& inRenderer ) noexcept
    : m_Renderer( inRenderer )
    {}

    /** @brief Loads inPath as the active scene and resolves its Sprite/Animation assets through m_Renderer. */
    BoolResult LoadScene( str::String const& inPath ) noexcept
    {
        auto result = m_SceneManager.LoadScene( inPath );
        if ( !result ) return result;
        m_Assets.ResolveAssets( m_SceneManager.GetRegistry(), m_Renderer );
        return result;
    }

    void Update( float inDeltaTime, input::InputState const& inInput ) override
    {
        if ( auto transition = m_States.Update( inDeltaTime, inInput ) )
            ApplyTransition( *transition );
    }

    void Render( video::IRenderer& inRenderer ) override
    {
        m_States.Render( inRenderer );
    }

    void OnSystemEvent( event::SystemEvent const& inSysEvent ) override
    {
        m_States.OnSystemEvent( inSysEvent );
    }
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

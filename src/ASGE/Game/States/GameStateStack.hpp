#pragma once

#include <vector>
#include <memory>
#include "GameState.hpp"

namespace asge::game::state
{

/**
 * @brief A stack of non-owning IGameState pointers driving Update/Render/event dispatch.
 *
 * Ownership stays with the caller (typically Game<TStateId>'s state cache) —
 * PushRaw/PopRaw/ReplaceRaw only ever store/erase raw pointers and fire
 * OnEnter/OnExit at the right time. See IGameState's doc comment for how
 * Update, Render, and OnSystemEvent each traverse the stack differently.
 */
template<typename TStateId>
class GameStateStack
{
public:
    using StateType  = IGameState<TStateId>;
    using Transition = Transition<TStateId>;

private:
    std::vector<StateType*> m_States; // Non-owning, states are owned by the game itself

public:
    /** @brief Calls inState->OnEnter() and pushes it as the new topmost state. */
    void PushRaw( StateType* inState ) noexcept
    {
        inState->OnEnter();
        m_States.push_back( inState );
    }

    /** @brief Calls the topmost state's OnExit() and pops it. No-op on an empty stack. */
    void PopRaw() noexcept
    {
        if ( m_States.empty() ) return;
        m_States.back()->OnExit();
        m_States.pop_back();
    }

    /** @brief Pops the current top (if any) and pushes inState in its place, at the same depth. */
    void ReplaceRaw( StateType* inState ) noexcept
    {
        PopRaw();
        PushRaw( inState );
    }

    /**
     * @brief Updates states top-down, stopping after the first whose
     *        BlocksUpdateBelow() is true; the topmost requested transition wins.
     */
    [[nodiscard]] std::optional<Transition> Update(
        float inDeltaTime, input::InputState const& inInput ) noexcept
    {
        std::optional<Transition> result;
        for ( auto it = m_States.rbegin(); it != m_States.rend(); ++it )
        {
            auto requested = (*it)->Update( inDeltaTime, inInput );
            if ( !result && requested ) result = requested; // topmost request wins if more than one fires
            if ( (*it)->BlocksUpdateBelow() ) break;
        }
        return result;
    }

    /**
     * @brief Renders bottom-up over the visible range: walks down from the top
     *        while each state's RendersBelow() lets what's under it show
     *        through, then draws that range back up so layering is correct.
     */
    void Render( video::IRenderer& inRenderer ) noexcept
    {
        if ( m_States.empty() ) return;

        std::size_t start = m_States.size() - 1;
        while ( start > 0 && m_States[start]->RendersBelow() ) --start;

        for ( std::size_t i = start; i < m_States.size(); ++i )
        {
            m_States[i]->Render( inRenderer );
        }
    }

    /** @brief Dispatches inSysEvent to the topmost state only. No-op on an empty stack. */
    void OnSystemEvent( event::SystemEvent const& inSysEvent ) noexcept
    {
        if ( !m_States.empty() ) m_States.back()->OnSystemEvent( inSysEvent ); // top only
    }

    /** @brief True if the stack has no states. */
    [[nodiscard]] bool Empty() const noexcept { return m_States.empty(); }
};

}

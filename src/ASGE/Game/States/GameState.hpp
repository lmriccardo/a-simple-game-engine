#pragma once

#include <optional>
#include <ASGE/Input/InputState.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Events/Events.hpp>

namespace asge::game::state
{

/** @brief What a state Update() is asking GameStateStack to do to the stack. */
enum class TransitionKind { None, Push, Pop, Replace };

/** @brief One requested stack change: push/replace a new state, or pop the current one. */
template<typename TStateId>
struct Transition
{
    TStateId        m_TargetId; // Ignored when m_Kind == Pop
    TransitionKind  m_Kind;
};

/**
 * @brief One node of a game's state stack (menu, gameplay, pause overlay, ...).
 *
 * GameStateStack owns the traversal: it calls Update() top-down (stopping at
 * the first state whose BlocksUpdateBelow() is true) and Render() bottom-up
 * over however many states RendersBelow() lets show through, dispatching
 * OnSystemEvent() to the topmost state only. OnEnter()/OnExit() bracket a
 * state's time at the top of a Push/Pop/Replace.
 */
template<typename TStateId>
class IGameState
{
public:
    virtual ~IGameState() = default;

    /** @brief Advances this state by one frame; an engaged result requests a stack transition. */
    [[nodiscard]] virtual std::optional<Transition<TStateId>>
    Update( float inDeltaTime, input::InputState const& inInput ) = 0;

    /** @brief Draws this state's frame. */
    virtual void Render( video::IRenderer& inRenderer ) = 0;

    /** @brief Called for every system event while this state is topmost. */
    virtual void OnSystemEvent( event::SystemEvent const& inSysEvent ) = 0;

    /** @brief Called once when this state becomes topmost via Push/Replace. */
    virtual void OnEnter() {}
    /** @brief Called once when this state stops being topmost via Pop/Replace. */
    virtual void OnExit() {}

    /** @brief True (the default) if states below this one should not receive Update(). */
    [[nodiscard]] virtual bool BlocksUpdateBelow() const noexcept { return true; }
    /** @brief True if this state is drawn over whatever is below it rather than covering it. */
    [[nodiscard]] virtual bool RendersBelow()      const noexcept { return false; }
};

}

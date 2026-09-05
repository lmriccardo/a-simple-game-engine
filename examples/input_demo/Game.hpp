#pragma once

#include <ASGE/ASGE.hpp>
#include <array>

/**
 * @brief Step 3 of the input-system roadmap: a small InputState/InputSystem showcase.
 *
 * Everything here is driven by polling InputState in Update() -- WASD movement,
 * a SPACE toggle (edge-triggered), a mouse cursor, left-click-held vs. right-click
 * (edge-triggered) and scroll -- with OnSystemEvent left empty, proving the polling
 * API is enough on its own for a real game loop.
 */
class InputDemoState final : public asge::game::state::IGameState<int>
{
    static constexpr std::size_t kMaxMarks = 24;

    asge::math::Float2 m_BoxPos{ 380.0f, 280.0f };
    float               m_BoxSize{ 48.0f };
    bool                m_LightBackground{ false };

    // Render() only gets an IRenderer, not InputState, so the cursor's
    // position/held-state are cached here in Update() and just drawn in Render().
    asge::math::Float2 m_CursorPos{};
    bool                m_CursorHeld{ false };

    std::array<asge::math::Float2, kMaxMarks> m_Marks{};
    std::size_t m_MarkCount{ 0 };
    std::size_t m_NextMarkSlot{ 0 };

    void UpdatePlayerBox(float inDeltaTime, asge::input::InputState const& inInput);
    void UpdateBackgroundToggle(asge::input::InputState const& inInput);
    void UpdateBoxSizeFromScroll(asge::input::InputState const& inInput);
    void UpdateCursor(asge::input::InputState const& inInput);
    void UpdateMarks(asge::input::InputState const& inInput);

public:
    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class InputDemoGame final : public asge::game::Game<int>
{
public:
    explicit InputDemoGame(asge::video::IRenderer& inRenderer);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};

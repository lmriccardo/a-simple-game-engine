#include <ASGE/Game/Game.hpp>

#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

namespace
{

using asge::game::Game;
using asge::game::state::IGameState;
using asge::game::state::Transition;
using asge::game::state::TransitionKind;

// Records every call it receives, and lets a test arm its next Update()'s
// requested transition directly -- see TestGame::m_Created below.
class MockGameState final : public IGameState<int>
{
public:
    int  m_Id{0};
    int  m_EnterCount{0};
    int  m_ExitCount{0};
    int  m_EventCount{0};
    bool m_BlocksUpdateBelow{true};
    std::optional<Transition<int>> m_NextTransition{};
    std::vector<int>*              m_RenderOrder{nullptr};

    [[nodiscard]] std::optional<Transition<int>> Update(
        float, asge::input::InputState const&) override
    {
        return m_NextTransition;
    }

    void Render(asge::video::IRenderer&) override
    {
        if (m_RenderOrder) m_RenderOrder->push_back(m_Id);
    }

    void OnSystemEvent(asge::event::SystemEvent const&) override { ++m_EventCount; }
    void OnEnter() override { ++m_EnterCount; }
    void OnExit()  override { ++m_ExitCount; }

    [[nodiscard]] bool BlocksUpdateBelow() const noexcept override { return m_BlocksUpdateBelow; }
};

class NullRenderer final : public asge::video::IRenderer
{
public:
    void Clear(asge::media::RGBA_Color const&) const override {}
    void DrawRect(asge::math::Rect const&, asge::media::RGBA_Color const&, bool) const override {}
    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::media::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const&, int, asge::media::RGBA_Color const&, bool) const override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const&) const noexcept override {}
    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawString(asge::str::StringView, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::media::RGBA_Color const&) const noexcept override {}
    void Present() const override {}
    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const override { return true; }
};

// Exposes Game<int>'s protected members for testing, and tracks CreateState
// calls/instances by id so a test can both assert on caching and reach into
// a state already on the stack to arm its next requested transition.
class TestGame final : public Game<int>
{
public:
    using Game::Game;
    using Game::SetInitialState;
    using Game::InvalidateState;

    std::unordered_map<int, MockGameState*> m_Created;
    std::unordered_map<int, int>            m_CreateCount;

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState( int inId ) override
    {
        auto state = std::make_unique<MockGameState>();
        state->m_Id = inId;
        m_Created[inId] = state.get();
        ++m_CreateCount[inId];
        return state;
    }
};

class GameTest : public ::testing::Test
{
protected:
    NullRenderer m_Renderer;
    TestGame     m_Game{ m_Renderer };
    asge::input::InputState m_Input;
};

// ─── SetInitialState ─────────────────────────────────────────────────────────

TEST_F(GameTest, SetInitialState_CreatesAndEntersStateOnce)
{
    m_Game.SetInitialState(0);

    ASSERT_EQ(m_Game.m_CreateCount[0], 1);
    EXPECT_EQ(m_Game.m_Created[0]->m_EnterCount, 1);
}

// ─── Update -> transition wiring ─────────────────────────────────────────────

TEST_F(GameTest, Update_NoTransitionRequested_StateIsNotRecreated)
{
    m_Game.SetInitialState(0);

    m_Game.Update(0.016f, m_Input);
    m_Game.Update(0.016f, m_Input);

    EXPECT_EQ(m_Game.m_CreateCount[0], 1);
}

TEST_F(GameTest, Update_PushTransition_CreatesAndEntersTargetKeepingOldOnStack)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };

    m_Game.Update(0.016f, m_Input);

    ASSERT_EQ(m_Game.m_CreateCount[1], 1);
    EXPECT_EQ(m_Game.m_Created[1]->m_EnterCount, 1);
    EXPECT_EQ(m_Game.m_Created[0]->m_ExitCount, 0); // still on the stack, just not topmost
}

TEST_F(GameTest, Update_PopTransition_ExitsTopAndReturnsToPrevious)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_Game.Update(0.016f, m_Input); // [0] -> [0, 1]

    m_Game.m_Created[1]->m_NextTransition = Transition<int>{ 0, TransitionKind::Pop };
    m_Game.Update(0.016f, m_Input); // [0, 1] -> [0]

    EXPECT_EQ(m_Game.m_Created[1]->m_ExitCount, 1);

    // Only state 0 should be left on the stack -- events reach it again.
    m_Game.OnSystemEvent(asge::event::SystemEvent{});
    EXPECT_EQ(m_Game.m_Created[0]->m_EventCount, 1);
}

TEST_F(GameTest, Update_ReplaceTransition_ExitsOldEntersNewAtSameDepth)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Replace };

    m_Game.Update(0.016f, m_Input);

    EXPECT_EQ(m_Game.m_Created[0]->m_ExitCount, 1);
    EXPECT_EQ(m_Game.m_Created[1]->m_EnterCount, 1);

    // Replace keeps the stack depth at one -- an event now reaches only state 1.
    m_Game.OnSystemEvent(asge::event::SystemEvent{});
    EXPECT_EQ(m_Game.m_Created[1]->m_EventCount, 1);
    EXPECT_EQ(m_Game.m_Created[0]->m_EventCount, 0);
}

// ─── State caching ────────────────────────────────────────────────────────────

TEST_F(GameTest, GetOrCreateState_SameIdReusedAcrossPopAndPushAgain)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_Game.Update(0.016f, m_Input); // 0 -> [0, 1]
    m_Game.m_Created[1]->m_NextTransition = Transition<int>{ 0, TransitionKind::Pop };
    m_Game.Update(0.016f, m_Input); // [0, 1] -> [0]
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_Game.Update(0.016f, m_Input); // [0] -> [0, 1] again

    EXPECT_EQ(m_Game.m_CreateCount[1], 1); // never recreated
}

TEST_F(GameTest, InvalidateState_ForcesRecreationOnNextTransitionToIt)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_Game.Update(0.016f, m_Input);
    ASSERT_EQ(m_Game.m_CreateCount[1], 1);

    m_Game.m_Created[1]->m_NextTransition = Transition<int>{ 0, TransitionKind::Pop };
    m_Game.Update(0.016f, m_Input);
    m_Game.InvalidateState(1);

    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_Game.Update(0.016f, m_Input);

    EXPECT_EQ(m_Game.m_CreateCount[1], 2);
}

// ─── OnSystemEvent ────────────────────────────────────────────────────────────

TEST_F(GameTest, OnSystemEvent_ForwardsToTopmostState)
{
    m_Game.SetInitialState(0);

    m_Game.OnSystemEvent(asge::event::SystemEvent{});

    EXPECT_EQ(m_Game.m_Created[0]->m_EventCount, 1);
}

}

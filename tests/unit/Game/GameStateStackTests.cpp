#include <ASGE/Game/States/GameStateStack.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace
{

using asge::game::state::GameStateStack;
using asge::game::state::IGameState;
using asge::game::state::Transition;
using asge::game::state::TransitionKind;

// Records every call it receives instead of doing anything real, so tests
// can assert on stack traversal order without a real renderer/game state.
class MockGameState final : public IGameState<int>
{
public:
    int  m_Id{0};
    int  m_EnterCount{0};
    int  m_ExitCount{0};
    int  m_UpdateCount{0};
    int  m_EventCount{0};
    bool m_BlocksUpdateBelow{true};
    bool m_RendersBelow{false};
    std::optional<Transition<int>> m_NextTransition{};
    std::vector<int>*              m_RenderOrder{nullptr}; // shared log; m_Id appended on Render()

    [[nodiscard]] std::optional<Transition<int>> Update(
        float, asge::input::InputState const&) override
    {
        ++m_UpdateCount;
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
    [[nodiscard]] bool RendersBelow()      const noexcept override { return m_RendersBelow; }
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

class GameStateStackTest : public ::testing::Test
{
protected:
    GameStateStack<int> m_Stack;
    MockGameState        m_A;
    MockGameState        m_B;
    NullRenderer         m_Renderer;
    asge::input::InputState m_Input;
};

// ─── PushRaw / PopRaw / ReplaceRaw ───────────────────────────────────────────

TEST_F(GameStateStackTest, PushRaw_EntersStateAndAddsToStack)
{
    EXPECT_TRUE(m_Stack.Empty());

    m_Stack.PushRaw(&m_A);

    EXPECT_FALSE(m_Stack.Empty());
    EXPECT_EQ(m_A.m_EnterCount, 1);
    EXPECT_EQ(m_A.m_ExitCount, 0);
}

TEST_F(GameStateStackTest, PopRaw_ExitsTopStateAndRemovesFromStack)
{
    m_Stack.PushRaw(&m_A);

    m_Stack.PopRaw();

    EXPECT_TRUE(m_Stack.Empty());
    EXPECT_EQ(m_A.m_ExitCount, 1);
}

TEST_F(GameStateStackTest, PopRaw_EmptyStack_IsNoOp)
{
    m_Stack.PopRaw();

    EXPECT_TRUE(m_Stack.Empty());
}

TEST_F(GameStateStackTest, ReplaceRaw_ExitsOldEntersNew_StackDepthUnchanged)
{
    m_Stack.PushRaw(&m_A);

    m_Stack.ReplaceRaw(&m_B);

    EXPECT_FALSE(m_Stack.Empty());
    EXPECT_EQ(m_A.m_ExitCount, 1);
    EXPECT_EQ(m_B.m_EnterCount, 1);

    std::vector<int> order;
    m_A.m_RenderOrder = &order;
    m_B.m_RenderOrder = &order;
    m_Stack.Render(m_Renderer);
    EXPECT_EQ(order, (std::vector<int>{ m_B.m_Id }));
}

TEST_F(GameStateStackTest, ReplaceRaw_EmptyStack_ActsLikePush)
{
    m_Stack.ReplaceRaw(&m_A);

    EXPECT_FALSE(m_Stack.Empty());
    EXPECT_EQ(m_A.m_EnterCount, 1);
}

// ─── Update ──────────────────────────────────────────────────────────────────

TEST_F(GameStateStackTest, Update_TopmostRequestedTransitionWins)
{
    m_B.m_BlocksUpdateBelow = false; // topmost -- let Update walk down to m_A too
    m_A.m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_B.m_NextTransition = Transition<int>{ 2, TransitionKind::Push };
    m_Stack.PushRaw(&m_A);
    m_Stack.PushRaw(&m_B); // B is topmost

    auto result = m_Stack.Update(0.016f, m_Input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->m_TargetId, 2);
    EXPECT_EQ(m_A.m_UpdateCount, 1);
    EXPECT_EQ(m_B.m_UpdateCount, 1);
}

TEST_F(GameStateStackTest, Update_BlockingTopState_StopsPropagationToLowerStates)
{
    m_B.m_BlocksUpdateBelow = true; // default, but explicit here
    m_Stack.PushRaw(&m_A);
    m_Stack.PushRaw(&m_B);

    auto result = m_Stack.Update(0.016f, m_Input);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(m_B.m_UpdateCount, 1);
    EXPECT_EQ(m_A.m_UpdateCount, 0);
}

TEST_F(GameStateStackTest, Update_NonBlockingTopState_LowerStatesTransitionIsUsedWhenTopRequestsNone)
{
    m_B.m_BlocksUpdateBelow = false;
    m_A.m_NextTransition = Transition<int>{ 5, TransitionKind::Pop };
    m_Stack.PushRaw(&m_A);
    m_Stack.PushRaw(&m_B); // B requests nothing

    auto result = m_Stack.Update(0.016f, m_Input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->m_TargetId, 5);
    EXPECT_EQ(m_A.m_UpdateCount, 1);
    EXPECT_EQ(m_B.m_UpdateCount, 1);
}

// ─── Render ──────────────────────────────────────────────────────────────────

TEST_F(GameStateStackTest, Render_EmptyStack_NoStatesRendered)
{
    std::vector<int> order;
    m_A.m_RenderOrder = &order;

    m_Stack.Render(m_Renderer);

    EXPECT_TRUE(order.empty());
}

TEST_F(GameStateStackTest, Render_OpaqueTopState_OnlyRendersTop)
{
    m_A.m_Id = 1;
    m_B.m_Id = 2;
    m_B.m_RendersBelow = false; // opaque -- default
    m_Stack.PushRaw(&m_A);
    m_Stack.PushRaw(&m_B);

    std::vector<int> order;
    m_A.m_RenderOrder = &order;
    m_B.m_RenderOrder = &order;
    m_Stack.Render(m_Renderer);

    EXPECT_EQ(order, (std::vector<int>{ 2 }));
}

TEST_F(GameStateStackTest, Render_TransparentTopState_RendersBothBottomToTop)
{
    m_A.m_Id = 1;
    m_B.m_Id = 2;
    m_B.m_RendersBelow = true; // e.g. a translucent pause overlay
    m_Stack.PushRaw(&m_A);
    m_Stack.PushRaw(&m_B);

    std::vector<int> order;
    m_A.m_RenderOrder = &order;
    m_B.m_RenderOrder = &order;
    m_Stack.Render(m_Renderer);

    EXPECT_EQ(order, (std::vector<int>{ 1, 2 }));
}

// ─── OnSystemEvent ───────────────────────────────────────────────────────────

TEST_F(GameStateStackTest, OnSystemEvent_DispatchedToTopStateOnly)
{
    m_Stack.PushRaw(&m_A);
    m_Stack.PushRaw(&m_B);

    m_Stack.OnSystemEvent(asge::event::SystemEvent{});

    EXPECT_EQ(m_B.m_EventCount, 1);
    EXPECT_EQ(m_A.m_EventCount, 0);
}

TEST_F(GameStateStackTest, OnSystemEvent_EmptyStack_IsNoOp)
{
    m_Stack.OnSystemEvent(asge::event::SystemEvent{});
    SUCCEED();
}

}

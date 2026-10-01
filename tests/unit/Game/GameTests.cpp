#include <ASGE/Game/Game.hpp>
#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Velocity.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Game/Scene/SceneSerializer.hpp>
#include <filesystem>

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

    void RenderBackground(asge::video::IRenderer&) override
    {
        if (m_RenderOrder) m_RenderOrder->push_back(-1 - m_Id);
    }

    [[nodiscard]] asge::graphics::RGBA_Color ClearColor() const noexcept override { return m_Clear; }

    asge::graphics::RGBA_Color m_Clear{ 0, 0, 0, 255 };

    void OnSystemEvent(asge::event::SystemEvent const&) override { ++m_EventCount; }
    void OnEnter() override { ++m_EnterCount; }
    void OnExit()  override { ++m_ExitCount; }

    [[nodiscard]] bool BlocksUpdateBelow() const noexcept override { return m_BlocksUpdateBelow; }
};

class NullRenderer final : public asge::video::IRenderer
{
public:
    void Clear(asge::graphics::RGBA_Color const& inColor) const override { m_LastClear = inColor; }
    void DrawRect(asge::math::Rect const&, asge::graphics::RGBA_Color const&, bool) const override {}
    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::graphics::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const&, int, asge::graphics::RGBA_Color const&, bool) const override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const&) const noexcept override {}
    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Rect const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawString(asge::str::StringView, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::graphics::RGBA_Color const&) const noexcept override {}
    void Present() const override {}
    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const override { return true; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }

    void SetCamera(asge::video::Camera const& inCamera) override { m_Camera = inCamera; }
    [[nodiscard]] asge::video::Camera const& GetCamera() const override { return m_Camera; }
    void SetViewport(asge::video::Viewport const& inViewport) override { m_Viewport = inViewport; }
    [[nodiscard]] asge::video::Viewport const& GetViewport() const override { return m_Viewport; }

    mutable asge::graphics::RGBA_Color m_LastClear{};

private:
    asge::video::Camera   m_Camera{};
    asge::video::Viewport m_Viewport{};
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
    using Game::m_SceneManager;
    using Game::m_Vfs;

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
    NullRenderer            m_Renderer;
    asge::audio::AudioDevice m_AudioDev; // never Initialize()'d -- Game doesn't need a live device to be tested
    TestGame                m_Game{ m_Renderer, m_AudioDev };
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

// ─── QuitRequested ────────────────────────────────────────────────────────────

TEST_F(GameTest, QuitRequested_FreshGame_IsFalse)
{
    EXPECT_FALSE(m_Game.QuitRequested());
}

TEST_F(GameTest, Update_QuitTransition_SetsQuitRequestedTrue)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 0, TransitionKind::Quit };

    m_Game.Update(0.016f, m_Input);

    EXPECT_TRUE(m_Game.QuitRequested());
}

TEST_F(GameTest, NonQuitTransitions_NeverSetQuitRequested)
{
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_NextTransition = Transition<int>{ 1, TransitionKind::Push };
    m_Game.Update(0.016f, m_Input);
    EXPECT_FALSE(m_Game.QuitRequested());

    m_Game.m_Created[1]->m_NextTransition = Transition<int>{ 2, TransitionKind::Replace };
    m_Game.Update(0.016f, m_Input);
    EXPECT_FALSE(m_Game.QuitRequested());

    m_Game.m_Created[2]->m_NextTransition = Transition<int>{ 0, TransitionKind::Pop };
    m_Game.Update(0.016f, m_Input);
    EXPECT_FALSE(m_Game.QuitRequested());
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

TEST_F(GameTest, Construction_EnablesUIHitTesting)
{
    EXPECT_TRUE(m_Game.m_SceneManager.GetRegistry().GetResource<asge::game::resources::UIHitList>());
}

// ─── Per-frame system pipeline ───────────────────────────────────────────────

TEST_F(GameTest, Update_RunsMovementAndPropagationOverTheSceneRegistry)
{
    using asge::game::components::Transform;
    using asge::game::components::Velocity;

    m_Game.SetInitialState(0);
    auto& registry = m_Game.m_SceneManager.GetRegistry();
    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity);
    ASSERT_TRUE(registry.AddComponent<Transform>(entity.Value(), Transform{}));
    ASSERT_TRUE(registry.AddComponent<Velocity>(entity.Value(), Velocity{ .m_DX = 10.0f }));

    m_Game.Update(2.0f, m_Input);

    auto transform = registry.GetComponent<Transform>(entity.Value());
    ASSERT_TRUE(transform);
    EXPECT_FLOAT_EQ(transform.Value().get().m_LocalCoordinates.x(), 20.0f);
    EXPECT_FLOAT_EQ(transform.Value().get().m_WorldCoordinates.x(), 20.0f);
}

TEST_F(GameTest, Render_ClearsWithVisibleStatesColorThenDrawsBackgroundBeforeState)
{
    std::vector<int> order;
    m_Game.SetInitialState(0);
    m_Game.m_Created[0]->m_Clear = { 1, 2, 3, 255 };
    m_Game.m_Created[0]->m_RenderOrder = &order;

    m_Game.Render(m_Renderer);

    EXPECT_EQ(m_Renderer.m_LastClear.g, 2);
    EXPECT_EQ(order, (std::vector<int>{ -1, 0 }));
}

// ─── LoadProject ─────────────────────────────────────────────────────────────

class GameProjectTest : public GameTest
{
protected:
    std::filesystem::path m_Root;

    void SetUp() override
    {
        m_Root = std::filesystem::temp_directory_path()
            / ("asge_game_project_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(m_Root / "assets");
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Root, ec);
    }

    std::filesystem::path WriteProject(std::vector<std::string> const& inScenes, std::string const& inMain = {})
    {
        asge::config::toml::TOMLBuilder builder;
        auto mount = builder.ArrayTable("Mount");
        mount.Set("Name", std::string("assets"));
        mount.Set("RealDirectory", std::string("assets"));
        builder.SetArray("Scenes", inScenes);
        if (!inMain.empty()) builder.Set("MainScene", inMain);
        auto path = m_Root / "demo.asgeproject";
        EXPECT_TRUE(builder.SaveToFile(path).IsOk());
        return path;
    }

    void WriteScene(std::filesystem::path const& inPath, float inDX)
    {
        asge::ecs::Registry seed;
        auto entity = seed.CreateEntity();
        ASSERT_TRUE(entity.IsOk());
        ASSERT_TRUE(seed.AddComponent(entity.Value(), asge::game::components::Velocity{ inDX, 0.0f }).IsOk());
        asge::filesystem::VirtualFileSystem vfs;
        ASSERT_TRUE(asge::game::scene::SceneSerializer{ vfs }.Save(seed, inPath).IsOk());
    }
};

TEST_F(GameProjectTest, LoadProject_MountsVfsAndLoadsTheFirstScene)
{
    WriteScene(m_Root / "first.asgescene", 3.0f);
    WriteScene(m_Root / "second.asgescene", 9.0f);
    auto const project = WriteProject({ "first.asgescene", "second.asgescene" });

    ASSERT_TRUE(m_Game.LoadProject(project).IsOk());

    auto active = m_Game.m_SceneManager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(m_Game.m_SceneManager.GetRegistry()
        .GetComponent<asge::game::components::Velocity>(active[0]).Value().get().m_DX, 3.0f);
    EXPECT_EQ(m_Game.GetProject().m_Scenes.size(), 2u);
    EXPECT_EQ(m_Game.m_Vfs.ListMounts().size(), 1u);
}

TEST_F(GameProjectTest, LoadProject_MainSceneSet_LoadsItInsteadOfTheFirst)
{
    WriteScene(m_Root / "first.asgescene", 3.0f);
    WriteScene(m_Root / "second.asgescene", 9.0f);
    auto const project = WriteProject({ "first.asgescene", "second.asgescene" }, "second.asgescene");

    ASSERT_TRUE(m_Game.LoadProject(project).IsOk());

    auto active = m_Game.m_SceneManager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(m_Game.m_SceneManager.GetRegistry()
        .GetComponent<asge::game::components::Velocity>(active[0]).Value().get().m_DX, 9.0f);
}

TEST_F(GameProjectTest, LoadProject_MissingMountDirectory_IsSkippedNotFatal)
{
    WriteScene(m_Root / "first.asgescene", 1.0f);
    auto const project = WriteProject({ "first.asgescene" });
    std::filesystem::remove_all(m_Root / "assets");

    ASSERT_TRUE(m_Game.LoadProject(project).IsOk());
    EXPECT_TRUE(m_Game.m_Vfs.ListMounts().empty());
}

TEST_F(GameProjectTest, LoadProject_NoScenes_ReturnsEmptyProjectError)
{
    auto result = m_Game.LoadProject(WriteProject({}));

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(asge::errors::SceneError::EmptyProject));
}

TEST_F(GameProjectTest, LoadProject_MissingProjectFile_ReturnsError)
{
    EXPECT_FALSE(m_Game.LoadProject(m_Root / "nope.asgeproject").IsOk());
}

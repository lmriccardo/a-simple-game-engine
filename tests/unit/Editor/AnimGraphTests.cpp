#include "AnimGraph.hpp"

#include <ASGE/Game/Assets/AnimStateMachine.hpp>
#include <ASGE/Game/Assets/AnimationClipSet.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using asge::errors::AnimStateMachineErrors;

constexpr char const* kMachineXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<AnimStateMachine initial="idle">
  <Flags>
    <Flag name="running"/>
    <Flag name="on_ground"/>
  </Flags>
  <Triggers>
    <Trigger name="attack"/>
  </Triggers>
  <Clips>
    <Clip name="idle"/>
    <Clip name="run"/>
    <Clip name="attack"/>
  </Clips>
  <State name="idle" clip="idle" end="loop">
    <Transition to="attack" trigger="attack" priority="10"/>
    <Transition to="run" when="running !on_ground"/>
  </State>
  <State name="run" clip="run" end="loop">
    <Transition to="idle" when="!running"/>
  </State>
  <State name="attack" clip="attack" end="hold">
    <Transition to="idle" atEnd="true"/>
  </State>
</AnimStateMachine>
)";

class AnimGraphTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Dir;

    void SetUp() override
    {
        m_Dir = std::filesystem::temp_directory_path() / ("asge_animgraph_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(m_Dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Dir, ec);
    }

    std::filesystem::path Write(std::string const& inName, std::string const& inContent) const
    {
        auto const path = m_Dir / inName;
        std::ofstream file(path, std::ios::trunc);
        file << inContent;
        return path;
    }

    AnimMachineDoc Machine() const
    {
        auto doc = ReadMachine(Write("m.animsm", kMachineXml));
        EXPECT_TRUE(doc.IsOk());
        return doc.Value();
    }
};

// ─── ReadMachine ─────────────────────────────────────────────────────────────

TEST_F(AnimGraphTest, ReadMachine_ReadsNamesStatesAndTransitions)
{
    auto const doc = Machine();

    EXPECT_EQ(doc.m_Initial, "idle");
    ASSERT_EQ(doc.m_Flags.size(), 2u);
    EXPECT_EQ(doc.m_Flags[1], "on_ground");
    ASSERT_EQ(doc.m_Triggers.size(), 1u);
    ASSERT_EQ(doc.m_Clips.size(), 3u);
    ASSERT_EQ(doc.m_States.size(), 3u);
    EXPECT_EQ(doc.m_States[2].m_End, AnimEnd::Hold);

    auto const& idle = doc.m_States[0];
    ASSERT_EQ(idle.m_Transitions.size(), 2u);
    EXPECT_EQ(idle.m_Transitions[0].m_To, "attack");
    EXPECT_EQ(idle.m_Transitions[0].m_Trigger, "attack");
    EXPECT_EQ(idle.m_Transitions[0].m_Priority, 10);
    ASSERT_EQ(idle.m_Transitions[1].m_FlagsSet.size(), 1u);
    EXPECT_EQ(idle.m_Transitions[1].m_FlagsSet[0], "running");
    ASSERT_EQ(idle.m_Transitions[1].m_FlagsClear.size(), 1u);
    EXPECT_EQ(idle.m_Transitions[1].m_FlagsClear[0], "on_ground");
    EXPECT_TRUE(doc.m_States[2].m_Transitions[0].m_AtEnd);
}

TEST_F(AnimGraphTest, ReadMachine_MissingFileFails)
{
    EXPECT_FALSE(ReadMachine(m_Dir / "nope.animsm").IsOk());
}

TEST_F(AnimGraphTest, ReadMachine_OpensAFileTheEngineWouldReject)
{
    auto doc = ReadMachine(Write("bad.animsm",
        "<AnimStateMachine initial=\"ghost\">\n"
        "<State name=\"a\" clip=\"c\">\n"
        "<Transition to=\"nowhere\"/>\n"
        "</State>\n"
        "</AnimStateMachine>\n"));

    ASSERT_TRUE(doc.IsOk());
    EXPECT_EQ(doc.Value().m_Initial, "ghost");
    EXPECT_EQ(doc.Value().m_States[0].m_Transitions[0].m_To, "nowhere");
}

// ─── WriteMachine / ValidateMachine ──────────────────────────────────────────

TEST_F(AnimGraphTest, WriteMachine_ThenEngineLoadAcceptsIt)
{
    auto const path = m_Dir / "saved.animsm";

    ASSERT_TRUE(WriteMachine(Machine(), path).IsOk());

    auto loaded = asge::game::asset::AnimationStateMachine::Load(path);
    ASSERT_TRUE(loaded.IsOk());
    EXPECT_EQ(loaded.Value().States().size(), 3u);
    EXPECT_EQ(loaded.Value().TransitionsOf(0).size(), 2u);
}

TEST_F(AnimGraphTest, WriteMachine_ReadBackGivesTheSameModel)
{
    auto const original = Machine();
    auto const path = m_Dir / "saved.animsm";
    ASSERT_TRUE(WriteMachine(original, path).IsOk());

    auto read = ReadMachine(path);
    ASSERT_TRUE(read.IsOk());
    auto const& doc = read.Value();

    EXPECT_EQ(doc.m_Initial, original.m_Initial);
    EXPECT_EQ(doc.m_Flags, original.m_Flags);
    EXPECT_EQ(doc.m_Triggers, original.m_Triggers);
    EXPECT_EQ(doc.m_Clips, original.m_Clips);
    ASSERT_EQ(doc.m_States.size(), original.m_States.size());
    for (std::size_t ii = 0; ii < doc.m_States.size(); ++ii)
    {
        EXPECT_EQ(doc.m_States[ii].m_Name, original.m_States[ii].m_Name);
        EXPECT_EQ(doc.m_States[ii].m_Clip, original.m_States[ii].m_Clip);
        EXPECT_EQ(doc.m_States[ii].m_End, original.m_States[ii].m_End);
        ASSERT_EQ(doc.m_States[ii].m_Transitions.size(), original.m_States[ii].m_Transitions.size());
        for (std::size_t jj = 0; jj < doc.m_States[ii].m_Transitions.size(); ++jj)
        {
            auto const& a = doc.m_States[ii].m_Transitions[jj];
            auto const& b = original.m_States[ii].m_Transitions[jj];
            EXPECT_EQ(a.m_To, b.m_To);
            EXPECT_EQ(a.m_Trigger, b.m_Trigger);
            EXPECT_EQ(a.m_FlagsSet, b.m_FlagsSet);
            EXPECT_EQ(a.m_FlagsClear, b.m_FlagsClear);
            EXPECT_EQ(a.m_AtEnd, b.m_AtEnd);
            EXPECT_EQ(a.m_Priority, b.m_Priority);
        }
    }
}

TEST_F(AnimGraphTest, ValidateMachine_AcceptsAValidModel)
{
    EXPECT_TRUE(ValidateMachine(Machine()).IsOk());
}

TEST_F(AnimGraphTest, ValidateMachine_UnknownTargetStateIsStateNotFound)
{
    auto doc = Machine();
    doc.m_States[0].m_Transitions[0].m_To = "ghost";

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::StateNotFound));
}

TEST_F(AnimGraphTest, ValidateMachine_UnknownFlagIsFlagNotFound)
{
    auto doc = Machine();
    doc.m_States[1].m_Transitions[0].m_FlagsClear = { "ghost" };

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::FlagNotFound));
}

TEST_F(AnimGraphTest, ValidateMachine_UnknownTriggerIsTriggerNotFound)
{
    auto doc = Machine();
    doc.m_States[0].m_Transitions[0].m_Trigger = "ghost";

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::TriggerNotFound));
}

TEST_F(AnimGraphTest, ValidateMachine_UnknownClipIsClipNotFound)
{
    auto doc = Machine();
    doc.m_States[0].m_Clip = "ghost";

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::ClipNotFound));
}

TEST_F(AnimGraphTest, ValidateMachine_EmptyInitialIsMissingInitialState)
{
    auto doc = Machine();
    doc.m_Initial.clear();

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::MissingInitialState));
}

TEST_F(AnimGraphTest, ValidateMachine_EmptyStateNameIsMissingAttribute)
{
    auto doc = Machine();
    doc.m_States[1].m_Name.clear();

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::MissingAttribute));
}

TEST_F(AnimGraphTest, ValidateMachine_DuplicateFlagIsDuplicateName)
{
    auto doc = Machine();
    doc.m_Flags.push_back("running");

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::DuplicateName));
}

TEST_F(AnimGraphTest, ValidateMachine_MoreThan32FlagsIsLimitExceeded)
{
    auto doc = Machine();
    for (int ii = 0; ii < 40; ++ii) doc.m_Flags.push_back("f" + std::to_string(ii));

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::LimitExceeded));
}

TEST_F(AnimGraphTest, ValidateMachine_FlagBothSetAndClearIsInvalidAttribute)
{
    auto doc = Machine();
    doc.m_States[0].m_Transitions[1].m_FlagsClear = { "running" };

    auto result = ValidateMachine(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::InvalidAttribute));
}

TEST_F(AnimGraphTest, WriteMachine_InvalidModelFailsAndLeavesTheExistingFileAlone)
{
    auto const path = Write("keep.animsm", kMachineXml);
    auto doc = Machine();
    doc.m_States[0].m_Transitions[0].m_To = "ghost";

    auto result = WriteMachine(doc, path);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::StateNotFound));
    EXPECT_TRUE(asge::game::asset::AnimationStateMachine::Load(path).IsOk());
}

TEST_F(AnimGraphTest, WriteMachine_InvalidModelWritesNoNewFile)
{
    auto const path = m_Dir / "new.animsm";
    AnimMachineDoc doc;

    EXPECT_FALSE(WriteMachine(doc, path).IsOk());
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(AnimGraphTest, ValidateMachine_LeavesNoTempFileBehind)
{
    auto const countFiles = [] {
        std::size_t n = 0;
        for (auto const& e : std::filesystem::directory_iterator(std::filesystem::temp_directory_path()))
        {
            if (e.path().filename().string().starts_with("asge_anim_check_")) ++n;
        }
        return n;
    };
    auto const before = countFiles();

    (void)ValidateMachine(Machine());
    AnimMachineDoc broken;
    (void)ValidateMachine(broken);

    EXPECT_EQ(countFiles(), before);
}

// ─── Layout sidecar ──────────────────────────────────────────────────────────

TEST_F(AnimGraphTest, LayoutFilePath_AppendsLayoutNextToTheMachine)
{
    EXPECT_EQ(LayoutFilePath("anim/player.animsm"), std::filesystem::path("anim/player.animsm.layout"));
}

TEST_F(AnimGraphTest, Layout_SaveThenLoadKeepsEveryPosition)
{
    auto const path = m_Dir / "m.animsm";
    AnimLayout layout;
    layout["idle"] = { 10.0f, 20.5f };
    layout["run"] = { -30.0f, 40.0f };

    ASSERT_TRUE(SaveLayout(path, layout).IsOk());
    auto const loaded = LoadLayout(path);

    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_FLOAT_EQ(loaded.at("idle").m_X, 10.0f);
    EXPECT_FLOAT_EQ(loaded.at("idle").m_Y, 20.5f);
    EXPECT_FLOAT_EQ(loaded.at("run").m_X, -30.0f);
}

TEST_F(AnimGraphTest, Layout_MissingFileLoadsEmpty)
{
    EXPECT_TRUE(LoadLayout(m_Dir / "none.animsm").empty());
}

TEST_F(AnimGraphTest, Layout_UnreadableFileLoadsEmpty)
{
    auto const path = m_Dir / "m.animsm";
    Write("m.animsm.layout", "this is [not toml");

    EXPECT_TRUE(LoadLayout(path).empty());
}

TEST_F(AnimGraphTest, Layout_SavingAnEmptyLayoutRemovesTheFile)
{
    auto const path = m_Dir / "m.animsm";
    AnimLayout layout;
    layout["idle"] = { 1.0f, 2.0f };
    ASSERT_TRUE(SaveLayout(path, layout).IsOk());
    ASSERT_TRUE(std::filesystem::exists(LayoutFilePath(path)));

    ASSERT_TRUE(SaveLayout(path, {}).IsOk());

    EXPECT_FALSE(std::filesystem::exists(LayoutFilePath(path)));
}

TEST_F(AnimGraphTest, Layout_IsNotReadByTheEngineLoader)
{
    auto const path = m_Dir / "saved.animsm";
    ASSERT_TRUE(WriteMachine(Machine(), path).IsOk());
    AnimLayout layout;
    layout["idle"] = { 1.0f, 2.0f };
    ASSERT_TRUE(SaveLayout(path, layout).IsOk());

    EXPECT_TRUE(asge::game::asset::AnimationStateMachine::Load(path).IsOk());
}

TEST_F(AnimGraphTest, AutoLayout_PutsEveryStateInAColumnByDistanceFromInitial)
{
    AnimLayout layout;
    AutoLayout(Machine(), layout); // idle -> attack, idle -> run, run -> idle, attack -> idle

    ASSERT_EQ(layout.size(), 3u);
    EXPECT_FLOAT_EQ(layout.at("idle").m_X, 0.0f);
    EXPECT_GT(layout.at("attack").m_X, layout.at("idle").m_X);
    EXPECT_FLOAT_EQ(layout.at("attack").m_X, layout.at("run").m_X);
    EXPECT_NE(layout.at("attack").m_Y, layout.at("run").m_Y); // same column, different rows
}

TEST_F(AnimGraphTest, AutoLayout_KeepsExistingPositions)
{
    AnimLayout layout;
    layout["run"] = { 999.0f, 888.0f };

    AutoLayout(Machine(), layout);

    EXPECT_FLOAT_EQ(layout.at("run").m_X, 999.0f);
    EXPECT_FLOAT_EQ(layout.at("run").m_Y, 888.0f);
    EXPECT_EQ(layout.size(), 3u);
}

TEST_F(AnimGraphTest, AutoLayout_DropsStatesThatNoLongerExist)
{
    AnimLayout layout;
    layout["ghost"] = { 1.0f, 1.0f };

    AutoLayout(Machine(), layout);

    EXPECT_FALSE(layout.contains("ghost"));
}

TEST_F(AnimGraphTest, AutoLayout_UnreachableStateGoesInTheLastColumn)
{
    auto doc = Machine();
    doc.m_States.push_back({ "island", "idle", AnimEnd::Loop, {} });
    AnimLayout layout;

    AutoLayout(doc, layout);

    EXPECT_GT(layout.at("island").m_X, layout.at("attack").m_X);
}

TEST_F(AnimGraphTest, AutoLayout_EmptyInitialDoesNotCrash)
{
    auto doc = Machine();
    doc.m_Initial.clear();
    AnimLayout layout;

    AutoLayout(doc, layout);

    EXPECT_EQ(layout.size(), 3u);
}

// ─── Clip set ────────────────────────────────────────────────────────────────

TEST_F(AnimGraphTest, ClipSet_WriteThenEngineLoadAndReadBack)
{
    AnimClipSetDoc doc;
    doc.m_Machine = "anim/humanoid.animsm";
    doc.m_Clips = { { "idle", "anim/idle.toml", 0.15f }, { "run", "anim/run.toml", 0.1f } };
    auto const path = m_Dir / "set.animset";

    ASSERT_TRUE(WriteClipSet(doc, path).IsOk());

    auto loaded = asge::game::asset::AnimationClipSet::Load(path);
    ASSERT_TRUE(loaded.IsOk());
    EXPECT_EQ(loaded.Value().MachinePath(), "anim/humanoid.animsm");
    EXPECT_FLOAT_EQ(loaded.Value().Bindings()[0].m_FrameDuration, 0.15f);

    auto read = ReadClipSet(path);
    ASSERT_TRUE(read.IsOk());
    EXPECT_EQ(read.Value().m_Machine, "anim/humanoid.animsm");
    ASSERT_EQ(read.Value().m_Clips.size(), 2u);
    EXPECT_EQ(read.Value().m_Clips[1].m_Name, "run");
    EXPECT_EQ(read.Value().m_Clips[1].m_Path, "anim/run.toml");
    EXPECT_FLOAT_EQ(read.Value().m_Clips[0].m_FrameDuration, 0.15f);
}

TEST_F(AnimGraphTest, ClipSet_EmptyMachineIsMissingAttribute)
{
    AnimClipSetDoc doc;
    doc.m_Clips = { { "idle", "anim/idle.toml", 0.1f } };

    auto result = ValidateClipSet(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::MissingAttribute));
}

TEST_F(AnimGraphTest, ClipSet_EmptyClipPathIsMissingAttribute)
{
    AnimClipSetDoc doc;
    doc.m_Machine = "anim/m.animsm";
    doc.m_Clips = { { "idle", "", 0.1f } };

    auto result = ValidateClipSet(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::MissingAttribute));
}

TEST_F(AnimGraphTest, ClipSet_NonPositiveFrameDurationIsInvalidAttribute)
{
    AnimClipSetDoc doc;
    doc.m_Machine = "anim/m.animsm";
    doc.m_Clips = { { "idle", "anim/idle.toml", 0.0f } };

    auto result = ValidateClipSet(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::InvalidAttribute));
}

TEST_F(AnimGraphTest, ClipSet_DuplicateClipNameIsDuplicateName)
{
    AnimClipSetDoc doc;
    doc.m_Machine = "anim/m.animsm";
    doc.m_Clips = { { "idle", "anim/a.toml", 0.1f }, { "idle", "anim/b.toml", 0.1f } };

    auto result = ValidateClipSet(doc);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::DuplicateName));
}

TEST_F(AnimGraphTest, ClipSet_InvalidModelWritesNothing)
{
    auto const path = m_Dir / "bad.animset";

    EXPECT_FALSE(WriteClipSet(AnimClipSetDoc{}, path).IsOk());
    EXPECT_FALSE(std::filesystem::exists(path));
}

}

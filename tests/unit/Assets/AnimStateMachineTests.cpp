#include <ASGE/Game/Assets/AnimStateMachine.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using namespace asge::game::asset;
using asge::errors::AnimStateMachineErrors;

class AnimStateMachineTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Path;

    void SetUp() override
    {
        auto const uniqueName = "asge_animsm_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".xml";
        m_Path = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_Path, ec);
    }

    asge::Result<AnimationStateMachine> LoadText(std::string const& inContent) const
    {
        {
            std::ofstream file(m_Path, std::ios::trunc);
            file << inContent;
        }
        return AnimationStateMachine::Load(m_Path);
    }

    // Wraps inBody in a root element whose initial state is "a", with one clip "c".
    static std::string Doc(std::string const& inBody, std::string const& inInitial = "a")
    {
        return "<AnimStateMachine initial=\"" + inInitial + "\">\n"
               "<Clips>\n<Clip name=\"c\"/>\n</Clips>\n" + inBody + "</AnimStateMachine>\n";
    }

    // One state called "a" holding the given transitions, plus a second state "b".
    static std::string TwoStates(std::string const& inTransitions)
    {
        return "<Flags>\n<Flag name=\"f1\"/>\n<Flag name=\"f2\"/>\n</Flags>\n"
               "<Triggers>\n<Trigger name=\"t1\"/>\n</Triggers>\n"
               "<State name=\"a\" clip=\"c\">\n" + inTransitions + "</State>\n"
               "<State name=\"b\" clip=\"c\"/>\n";
    }

    void ExpectLoadError(std::string const& inContent, AnimStateMachineErrors inCode) const
    {
        auto result = LoadText(inContent);
        ASSERT_FALSE(result.IsOk());
        EXPECT_EQ(result.Code(), make_error_code(inCode));
    }
};

char const* const kValidXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!-- Player animation state machine -->
<AnimStateMachine initial="idle">
  <Flags>
    <Flag name="running"/>    <!-- bit 0 -->
    <Flag name="on_ground"/>  <!-- bit 1 -->
  </Flags>
  <Triggers>
    <Trigger name="attack"/>  <!-- bit 0 -->
    <Trigger name="hurt"/>    <!-- bit 1 -->
  </Triggers>
  <Clips>
    <Clip name="idle"/>
    <Clip name="run"/>
    <Clip name="jump"/>
    <Clip name="attack"/>
    <Clip name="hurt"/>
  </Clips>

  <State name="idle" clip="idle" end="loop">
    <Transition to="hurt"   trigger="hurt" priority="10"/>
    <Transition to="attack" trigger="attack"/>
    <Transition to="jump"   when="!on_ground"/>
    <Transition to="run"    when="running on_ground"/>
  </State>

  <State name="run" clip="run" end="loop">
    <Transition to="hurt"   trigger="hurt" priority="10"/>
    <Transition to="attack" trigger="attack"/>
    <Transition to="jump"   when="!on_ground"/>
    <Transition to="idle"   when="!running"/>
  </State>

  <State name="jump" clip="jump" end="hold">
    <Transition to="hurt" trigger="hurt" priority="10"/>
    <Transition to="run"  when="on_ground running"/>
    <Transition to="idle" when="on_ground !running"/>
  </State>

  <State name="attack" clip="attack" end="hold">
    <Transition to="hurt" trigger="hurt" priority="10"/>
    <Transition to="run"  atEnd="true" when="running on_ground" priority="1"/>
    <Transition to="idle" atEnd="true"/>
  </State>

  <State name="hurt" clip="hurt" end="restore">
    <Transition to="idle" atEnd="true"/>
  </State>
</AnimStateMachine>
)";

// ─── Load — valid documents ───────────────────────────────────────────────────

TEST_F(AnimStateMachineTest, Load_ValidDocumentBuildsAllStates)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto const& sm = result.Value();
    EXPECT_EQ(sm.States().size(), 5u);
    EXPECT_EQ(sm.Initial(), sm.FindState("idle").Value());
    EXPECT_EQ(sm.FindState("hurt").Value(), 4);
}

TEST_F(AnimStateMachineTest, Load_FlagsAndTriggersGetBitsInDeclarationOrder)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto const& sm = result.Value();
    EXPECT_EQ(sm.FindFlag("running").Value(), 0);
    EXPECT_EQ(sm.FindFlag("on_ground").Value(), 1);
    EXPECT_EQ(sm.FindTrigger("attack").Value(), 0);
    EXPECT_EQ(sm.FindTrigger("hurt").Value(), 1);
}

TEST_F(AnimStateMachineTest, Load_StateAttributesAreRead)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto const& sm = result.Value();
    auto states = sm.States();
    EXPECT_EQ(states[sm.FindState("idle").Value()].m_End, EndMode::Loop);
    EXPECT_EQ(states[sm.FindState("jump").Value()].m_End, EndMode::Hold);
    EXPECT_EQ(states[sm.FindState("hurt").Value()].m_End, EndMode::Restore);
}

TEST_F(AnimStateMachineTest, Load_TransitionsAreSortedByPriorityThenDocumentOrder)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto const& sm = result.Value();
    auto const idle = sm.FindState("idle").Value();
    auto transitions = sm.TransitionsOf(idle);
    ASSERT_EQ(transitions.size(), 4u);

    EXPECT_EQ(transitions[0].m_To, sm.FindState("hurt").Value());      // priority 10
    EXPECT_EQ(transitions[1].m_To, sm.FindState("attack").Value());   // document order
    EXPECT_EQ(transitions[2].m_To, sm.FindState("jump").Value());
    EXPECT_EQ(transitions[3].m_To, sm.FindState("run").Value());
}

TEST_F(AnimStateMachineTest, Load_TransitionConditionsBecomeBitMasks)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto const& sm = result.Value();
    auto idle = sm.TransitionsOf(sm.FindState("idle").Value());
    EXPECT_EQ(idle[0].m_Trigger, 1u << 1);       // hurt
    EXPECT_EQ(idle[1].m_Trigger, 1u << 0);       // attack
    EXPECT_EQ(idle[2].m_FlagsClear, 1u << 1);    // !on_ground
    EXPECT_EQ(idle[2].m_FlagsSet, 0u);
    EXPECT_EQ(idle[3].m_FlagsSet, 0b11u);        // running on_ground

    auto attack = sm.TransitionsOf(sm.FindState("attack").Value());
    ASSERT_EQ(attack.size(), 3u);
    EXPECT_EQ(attack[1].m_To, sm.FindState("run").Value());   // priority 1 beats 0
    EXPECT_TRUE(attack[1].m_AtEnd);
    EXPECT_TRUE(attack[2].m_AtEnd);
    EXPECT_FALSE(attack[0].m_AtEnd);
}

TEST_F(AnimStateMachineTest, Load_EndModeDefaultsToLoop)
{
    auto result = LoadText(Doc("<State name=\"a\" clip=\"c\"/>\n"));
    ASSERT_TRUE(result.IsOk());

    auto state = result.Value().States()[0];
    EXPECT_EQ(state.m_End, EndMode::Loop);
    EXPECT_TRUE(result.Value().TransitionsOf(0).empty());
}

TEST_F(AnimStateMachineTest, Load_FlagsAndTriggersSectionsAreOptional)
{
    EXPECT_TRUE(LoadText(Doc("<State name=\"a\" clip=\"c\"/>\n")).IsOk());
}

TEST_F(AnimStateMachineTest, Load_TransitionMayTargetAStateDeclaredLater)
{
    auto result = LoadText(Doc(TwoStates("<Transition to=\"b\"/>\n")));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().TransitionsOf(0)[0].m_To, 1);
}

// ─── Load — file and root errors ──────────────────────────────────────────────

TEST_F(AnimStateMachineTest, Load_MissingFileReturnsError)
{
    auto result = AnimationStateMachine::Load(m_Path); // never written
    ASSERT_FALSE(result.IsOk());
}

TEST_F(AnimStateMachineTest, Load_WrongRootElementReturnsUnexpectedElement)
{
    ExpectLoadError("<Other initial=\"a\"/>\n", AnimStateMachineErrors::UnexpectedElement);
}

TEST_F(AnimStateMachineTest, Load_MissingInitialReturnsMissingInitialState)
{
    ExpectLoadError("<AnimStateMachine>\n<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n",
        AnimStateMachineErrors::MissingInitialState);
}

TEST_F(AnimStateMachineTest, Load_BlankInitialReturnsMissingInitialState)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\"/>\n", "  "),
        AnimStateMachineErrors::MissingInitialState);
}

TEST_F(AnimStateMachineTest, Load_UnknownInitialReturnsStateNotFound)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\"/>\n", "ghost"),
        AnimStateMachineErrors::StateNotFound);
}

TEST_F(AnimStateMachineTest, Load_UnknownRootChildReturnsUnexpectedElement)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\"/>\n<Bogus/>\n"),
        AnimStateMachineErrors::UnexpectedElement);
}

TEST_F(AnimStateMachineTest, Load_NoStatesReturnsStateNotFound)
{
    ExpectLoadError(Doc("<Flags>\n<Flag name=\"f\"/>\n</Flags>\n"),
        AnimStateMachineErrors::StateNotFound);
}

// ─── Load — flags and triggers ────────────────────────────────────────────────

TEST_F(AnimStateMachineTest, Load_FlagWithoutNameReturnsMissingAttribute)
{
    ExpectLoadError(Doc("<Flags>\n<Flag/>\n</Flags>\n<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimStateMachineTest, Load_FlagWithBlankNameReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<Flags>\n<Flag name=\"  \"/>\n</Flags>\n<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_DuplicateFlagReturnsDuplicateName)
{
    ExpectLoadError(Doc("<Flags>\n<Flag name=\"f\"/>\n<Flag name=\"f\"/>\n</Flags>\n"
                        "<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::DuplicateName);
}

TEST_F(AnimStateMachineTest, Load_DuplicateTriggerReturnsDuplicateName)
{
    ExpectLoadError(Doc("<Triggers>\n<Trigger name=\"t\"/>\n<Trigger name=\"t\"/>\n</Triggers>\n"
                        "<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::DuplicateName);
}

TEST_F(AnimStateMachineTest, Load_FlagNameWithSpaceReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<Flags>\n<Flag name=\"two words\"/>\n</Flags>\n<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_FlagNameStartingWithBangReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<Flags>\n<Flag name=\"!f\"/>\n</Flags>\n<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_WrongElementInsideFlagsReturnsUnexpectedElement)
{
    ExpectLoadError(Doc("<Flags>\n<Trigger name=\"f\"/>\n</Flags>\n<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::UnexpectedElement);
}

TEST_F(AnimStateMachineTest, Load_MoreThan32FlagsReturnsLimitExceeded)
{
    std::string flags = "<Flags>\n";
    for (int i = 0; i < 33; ++i) flags += "<Flag name=\"f" + std::to_string(i) + "\"/>\n";
    flags += "</Flags>\n";

    ExpectLoadError(Doc(flags + "<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::LimitExceeded);
}

TEST_F(AnimStateMachineTest, Load_Exactly32FlagsSucceeds)
{
    std::string flags = "<Flags>\n";
    for (int i = 0; i < 32; ++i) flags += "<Flag name=\"f" + std::to_string(i) + "\"/>\n";
    flags += "</Flags>\n";

    EXPECT_TRUE(LoadText(Doc(flags + "<State name=\"a\" clip=\"c\"/>\n")).IsOk());
}

TEST_F(AnimStateMachineTest, Load_MoreThan32TriggersReturnsLimitExceeded)
{
    std::string triggers = "<Triggers>\n";
    for (int i = 0; i < 33; ++i) triggers += "<Trigger name=\"t" + std::to_string(i) + "\"/>\n";
    triggers += "</Triggers>\n";

    ExpectLoadError(Doc(triggers + "<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::LimitExceeded);
}

// ─── Load — states ────────────────────────────────────────────────────────────

TEST_F(AnimStateMachineTest, Load_StateWithoutNameReturnsMissingAttribute)
{
    ExpectLoadError(Doc("<State clip=\"c\"/>\n"), AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimStateMachineTest, Load_StateWithBlankNameReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<State name=\" \" clip=\"c\"/>\n"), AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_StateWithoutClipReturnsMissingAttribute)
{
    ExpectLoadError(Doc("<State name=\"a\"/>\n"), AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimStateMachineTest, Load_StateWithBlankClipReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"  \"/>\n"), AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_DuplicateStateReturnsDuplicateName)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\"/>\n<State name=\"a\" clip=\"c\"/>\n"),
        AnimStateMachineErrors::DuplicateName);
}

TEST_F(AnimStateMachineTest, Load_BlankEndReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\" end=\" \"/>\n"), AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_UnknownEndModeReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\" end=\"bounce\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_NonTransitionInsideStateReturnsUnexpectedElement)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"c\">\n<Flag name=\"x\"/>\n</State>\n"),
        AnimStateMachineErrors::UnexpectedElement);
}

// ─── Load — transitions ───────────────────────────────────────────────────────

TEST_F(AnimStateMachineTest, Load_TransitionWithoutToReturnsMissingAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition trigger=\"t1\"/>\n")),
        AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimStateMachineTest, Load_TransitionWithBlankToReturnsEmptyAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\" \"/>\n")), AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_TransitionToUnknownStateReturnsStateNotFound)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"ghost\"/>\n")),
        AnimStateMachineErrors::StateNotFound);
}

TEST_F(AnimStateMachineTest, Load_TransitionWithUnknownTriggerReturnsTriggerNotFound)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" trigger=\"ghost\"/>\n")),
        AnimStateMachineErrors::TriggerNotFound);
}

TEST_F(AnimStateMachineTest, Load_TransitionWithBlankTriggerReturnsEmptyAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" trigger=\" \"/>\n")),
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_TransitionWithUnknownFlagReturnsFlagNotFound)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" when=\"f1 ghost\"/>\n")),
        AnimStateMachineErrors::FlagNotFound);
}

TEST_F(AnimStateMachineTest, Load_TransitionWithBlankWhenReturnsEmptyAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" when=\" \"/>\n")),
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_WhenWithBareBangReturnsInvalidAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" when=\"f1 !\"/>\n")),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_WhenRequiringFlagSetAndClearReturnsInvalidAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" when=\"f1 !f1\"/>\n")),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_NonBooleanAtEndReturnsInvalidAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" atEnd=\"yes\"/>\n")),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_BlankAtEndReturnsEmptyAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" atEnd=\" \"/>\n")),
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_NonIntegerPriorityReturnsInvalidAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" priority=\"high\"/>\n")),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimStateMachineTest, Load_BlankPriorityReturnsEmptyAttribute)
{
    ExpectLoadError(Doc(TwoStates("<Transition to=\"b\" priority=\" \"/>\n")),
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_NegativePriorityOrdersAfterDefault)
{
    auto result = LoadText(Doc(TwoStates(
        "<Transition to=\"a\" priority=\"-5\"/>\n<Transition to=\"b\"/>\n")));
    ASSERT_TRUE(result.IsOk());

    auto transitions = result.Value().TransitionsOf(0);
    ASSERT_EQ(transitions.size(), 2u);
    EXPECT_EQ(transitions[0].m_To, 1);
    EXPECT_EQ(transitions[1].m_To, 0);
}

// ─── Load — clips ─────────────────────────────────────────────────────────────

TEST_F(AnimStateMachineTest, Load_ClipsGetIdsInDeclarationOrder)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto const& sm = result.Value();
    EXPECT_EQ(sm.FindClip("idle").Value(), 0);
    EXPECT_EQ(sm.FindClip("hurt").Value(), 4);
    EXPECT_EQ(sm.States()[sm.FindState("attack").Value()].m_ClipId, sm.FindClip("attack").Value());
}

TEST_F(AnimStateMachineTest, Load_StatesMayShareAClip)
{
    auto result = LoadText(Doc("<State name=\"a\" clip=\"c\"/>\n<State name=\"b\" clip=\"c\"/>\n"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().States()[0].m_ClipId, result.Value().States()[1].m_ClipId);
}

TEST_F(AnimStateMachineTest, Load_ClipsMayBeDeclaredAfterTheStates)
{
    auto result = LoadText(
        "<AnimStateMachine initial=\"a\">\n<State name=\"a\" clip=\"late\"/>\n"
        "<Clips>\n<Clip name=\"late\"/>\n</Clips>\n</AnimStateMachine>\n");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().States()[0].m_ClipId, 0);
}

TEST_F(AnimStateMachineTest, Load_StateWithUndeclaredClipReturnsClipNotFound)
{
    ExpectLoadError(Doc("<State name=\"a\" clip=\"ghost\"/>\n"), AnimStateMachineErrors::ClipNotFound);
}

TEST_F(AnimStateMachineTest, Load_MissingClipsSectionReturnsClipNotFound)
{
    ExpectLoadError("<AnimStateMachine initial=\"a\">\n<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n",
        AnimStateMachineErrors::ClipNotFound);
}

TEST_F(AnimStateMachineTest, Load_UnusedClipIsAllowed)
{
    EXPECT_TRUE(LoadText(
        "<AnimStateMachine initial=\"a\">\n<Clips>\n<Clip name=\"c\"/>\n<Clip name=\"spare\"/>\n</Clips>\n"
        "<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n").IsOk());
}

TEST_F(AnimStateMachineTest, Load_ClipWithoutNameReturnsMissingAttribute)
{
    ExpectLoadError(
        "<AnimStateMachine initial=\"a\">\n<Clips>\n<Clip/>\n</Clips>\n<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n",
        AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimStateMachineTest, Load_ClipWithBlankNameReturnsEmptyAttribute)
{
    ExpectLoadError(
        "<AnimStateMachine initial=\"a\">\n<Clips>\n<Clip name=\" \"/>\n</Clips>\n<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n",
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimStateMachineTest, Load_DuplicateClipReturnsDuplicateName)
{
    ExpectLoadError(
        "<AnimStateMachine initial=\"a\">\n<Clips>\n<Clip name=\"c\"/>\n<Clip name=\"c\"/>\n</Clips>\n"
        "<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n",
        AnimStateMachineErrors::DuplicateName);
}

TEST_F(AnimStateMachineTest, Load_WrongElementInsideClipsReturnsUnexpectedElement)
{
    ExpectLoadError(
        "<AnimStateMachine initial=\"a\">\n<Clips>\n<Flag name=\"c\"/>\n</Clips>\n<State name=\"a\" clip=\"c\"/>\n</AnimStateMachine>\n",
        AnimStateMachineErrors::UnexpectedElement);
}

TEST_F(AnimStateMachineTest, FindClip_UnknownNameReturnsClipNotFound)
{
    auto result = LoadText(kValidXml);
    ASSERT_TRUE(result.IsOk());

    auto clip = result.Value().FindClip("ghost");
    ASSERT_FALSE(clip.IsOk());
    EXPECT_EQ(clip.Code(), make_error_code(AnimStateMachineErrors::ClipNotFound));
}

} // namespace

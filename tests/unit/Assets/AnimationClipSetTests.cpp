#include <ASGE/Game/Assets/AnimationClipSet.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
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

class AnimationClipSetTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Path;

    void SetUp() override
    {
        auto const uniqueName = "asge_animclipset_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".xml";
        m_Path = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_Path, ec);
    }

    asge::Result<AnimationClipSet> LoadText(std::string const& inContent) const
    {
        {
            std::ofstream file(m_Path, std::ios::trunc);
            file << inContent;
        }
        return AnimationClipSet::Load(m_Path);
    }

    // Wraps inBody in a root element bound to a machine file.
    static std::string Doc(std::string const& inBody)
    {
        return "<AnimClipSet machine=\"anim/humanoid.animsm\">\n" + inBody + "</AnimClipSet>\n";
    }

    void ExpectLoadError(std::string const& inContent, AnimStateMachineErrors inCode) const
    {
        auto result = LoadText(inContent);
        ASSERT_FALSE(result.IsOk());
        EXPECT_EQ(result.Code(), make_error_code(inCode));
    }
};

// ─── Load — valid documents ───────────────────────────────────────────────────

TEST_F(AnimationClipSetTest, Load_ValidDocumentReadsEveryClip)
{
    auto result = LoadText(R"(<?xml version="1.0" encoding="UTF-8"?>
<AnimClipSet machine="anim/humanoid.animsm">
  <Clip name="idle"   path="anim/goblin_idle.toml"   frameDuration="0.15"/>
  <Clip name="run"    path="anim/goblin_run.toml"    frameDuration="0.10"/>
  <Clip name="attack" path="anim/goblin_attack.toml" frameDuration="0.07"/>
</AnimClipSet>
)");
    ASSERT_TRUE(result.IsOk());

    auto const& set = result.Value();
    EXPECT_EQ(set.MachinePath(), "anim/humanoid.animsm");
    ASSERT_EQ(set.Bindings().size(), 3u);
    ASSERT_EQ(set.ClipNames().size(), 3u);
    ASSERT_EQ(set.ClipPaths().size(), 3u);

    EXPECT_EQ(set.ClipNames()[1], "run");
    EXPECT_EQ(set.ClipPaths()[2], "anim/goblin_attack.toml");
    EXPECT_FLOAT_EQ(set.Bindings()[0].m_FrameDuration, 0.15f);
    EXPECT_FLOAT_EQ(set.Bindings()[2].m_FrameDuration, 0.07f);
}

TEST_F(AnimationClipSetTest, Load_BindingsStartUnresolved)
{
    auto result = LoadText(Doc("<Clip name=\"idle\" path=\"idle.toml\"/>\n"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().Bindings()[0].m_Clip, nullptr);
}

TEST_F(AnimationClipSetTest, Load_FrameDurationDefaultsWhenOmitted)
{
    auto result = LoadText(Doc("<Clip name=\"idle\" path=\"idle.toml\"/>\n"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_FLOAT_EQ(result.Value().Bindings()[0].m_FrameDuration, 0.1f);
}

TEST_F(AnimationClipSetTest, Load_EmptyClipSetIsAllowed)
{
    auto result = LoadText(Doc(""));
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().Bindings().empty());
}

TEST_F(AnimationClipSetTest, Load_ClipsKeepDeclarationOrder)
{
    auto result = LoadText(Doc(
        "<Clip name=\"b\" path=\"b.toml\"/>\n<Clip name=\"a\" path=\"a.toml\"/>\n"));
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().ClipNames()[0], "b");
    EXPECT_EQ(result.Value().ClipPaths()[1], "a.toml");
}

// ─── Load — errors ────────────────────────────────────────────────────────────

TEST_F(AnimationClipSetTest, Load_MissingFileReturnsError)
{
    auto result = AnimationClipSet::Load(m_Path); // never written
    ASSERT_FALSE(result.IsOk());
}

TEST_F(AnimationClipSetTest, Load_WrongRootElementReturnsUnexpectedElement)
{
    ExpectLoadError("<AnimStateMachine machine=\"m\"/>\n", AnimStateMachineErrors::UnexpectedElement);
}

TEST_F(AnimationClipSetTest, Load_MissingMachineReturnsMissingAttribute)
{
    ExpectLoadError("<AnimClipSet>\n<Clip name=\"a\" path=\"a.toml\"/>\n</AnimClipSet>\n",
        AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimationClipSetTest, Load_BlankMachineReturnsEmptyAttribute)
{
    ExpectLoadError("<AnimClipSet machine=\"  \">\n<Clip name=\"a\" path=\"a.toml\"/>\n</AnimClipSet>\n",
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimationClipSetTest, Load_UnknownChildReturnsUnexpectedElement)
{
    ExpectLoadError(Doc("<State name=\"a\"/>\n"), AnimStateMachineErrors::UnexpectedElement);
}

TEST_F(AnimationClipSetTest, Load_ClipWithoutNameReturnsMissingAttribute)
{
    ExpectLoadError(Doc("<Clip path=\"a.toml\"/>\n"), AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimationClipSetTest, Load_ClipWithBlankNameReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<Clip name=\" \" path=\"a.toml\"/>\n"), AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimationClipSetTest, Load_ClipNameWithSpaceReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"two words\" path=\"a.toml\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimationClipSetTest, Load_ClipWithoutPathReturnsMissingAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"a\"/>\n"), AnimStateMachineErrors::MissingAttribute);
}

TEST_F(AnimationClipSetTest, Load_ClipWithBlankPathReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"a\" path=\"  \"/>\n"), AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimationClipSetTest, Load_DuplicateClipReturnsDuplicateName)
{
    ExpectLoadError(Doc("<Clip name=\"a\" path=\"a.toml\"/>\n<Clip name=\"a\" path=\"b.toml\"/>\n"),
        AnimStateMachineErrors::DuplicateName);
}

TEST_F(AnimationClipSetTest, Load_BlankFrameDurationReturnsEmptyAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"a\" path=\"a.toml\" frameDuration=\" \"/>\n"),
        AnimStateMachineErrors::EmptyAttribute);
}

TEST_F(AnimationClipSetTest, Load_NonNumericFrameDurationReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"a\" path=\"a.toml\" frameDuration=\"fast\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimationClipSetTest, Load_ZeroFrameDurationReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"a\" path=\"a.toml\" frameDuration=\"0\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

TEST_F(AnimationClipSetTest, Load_NegativeFrameDurationReturnsInvalidAttribute)
{
    ExpectLoadError(Doc("<Clip name=\"a\" path=\"a.toml\" frameDuration=\"-0.5\"/>\n"),
        AnimStateMachineErrors::InvalidAttribute);
}

// ─── ResolveAssets ────────────────────────────────────────────────────────────

class AnimationClipSetResolveTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Dir;
    asge::filesystem::VirtualFileSystem m_Vfs;

    static void WriteText(std::filesystem::path const& inPath, std::string const& inContent)
    {
        std::ofstream file(inPath, std::ios::trunc);
        file << inContent;
    }

    void SetUp() override
    {
        m_Dir = std::filesystem::temp_directory_path() / ("asge_animresolve_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(m_Dir);

        for (char const* name : { "idle", "run" })
        {
            WriteText(m_Dir / (std::string(name) + ".toml"),
                "[FrameTable]\nx = 0.0\ny = 0.0\nw = 8.0\nh = 8.0\ncolumns = 2\ncount = 4\n");
        }

        WriteText(m_Dir / "humanoid.animsm",
            "<AnimStateMachine initial=\"idle\">\n"
            "<Clips>\n<Clip name=\"idle\"/>\n<Clip name=\"run\"/>\n</Clips>\n"
            "<State name=\"idle\" clip=\"idle\"/>\n<State name=\"run\" clip=\"run\"/>\n"
            "</AnimStateMachine>\n");

        ASSERT_TRUE(m_Vfs.Mount("anim", m_Dir.string()).IsOk());
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Dir, ec);
    }

    // Loads a clip set bound to the humanoid machine with the given <Clip> lines.
    AnimationClipSet LoadSet(std::string const& inClips,
                             std::string const& inMachine = "anim/humanoid.animsm") const
    {
        auto const path = m_Dir / "set.xml";
        WriteText(path, "<AnimClipSet machine=\"" + inMachine + "\">\n" + inClips + "</AnimClipSet>\n");

        auto result = AnimationClipSet::Load(path);
        EXPECT_TRUE(result.IsOk());
        return result.IsOk() ? std::move(result).Value() : AnimationClipSet{};
    }
};

char const* const kBothClips =
    "<Clip name=\"idle\" path=\"anim/idle.toml\" frameDuration=\"0.2\"/>\n"
    "<Clip name=\"run\" path=\"anim/run.toml\" frameDuration=\"0.05\"/>\n";

TEST_F(AnimationClipSetResolveTest, ResolveAssets_LoadsMachineAndFrameTables)
{
    auto set = LoadSet(kBothClips);

    asge::game::asset::AssetManager mgr(m_Vfs);
    ASSERT_TRUE(set.ResolveAssets(mgr).IsOk());

    EXPECT_EQ(set.Machine().States().size(), 2u);
    ASSERT_EQ(set.Bindings().size(), 2u);
    ASSERT_NE(set.Bindings()[0].m_Clip, nullptr);
    EXPECT_EQ(set.Bindings()[0].m_Clip->VirtualPath(), "anim/idle.toml");
    EXPECT_EQ(set.Bindings()[1].m_Clip->Get().m_Frames.size(), 4u);
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_ReordersBindingsToTheMachineClipOrder)
{
    auto set = LoadSet(
        "<Clip name=\"run\" path=\"anim/run.toml\" frameDuration=\"0.05\"/>\n"
        "<Clip name=\"idle\" path=\"anim/idle.toml\" frameDuration=\"0.2\"/>\n");

    asge::game::asset::AssetManager mgr(m_Vfs);
    ASSERT_TRUE(set.ResolveAssets(mgr).IsOk());

    auto const& machine = set.Machine();
    auto const idleId = machine.FindClip("idle").Value();
    auto const runId = machine.FindClip("run").Value();

    EXPECT_EQ(set.ClipNames()[idleId], "idle");
    EXPECT_FLOAT_EQ(set.Bindings()[idleId].m_FrameDuration, 0.2f);
    EXPECT_FLOAT_EQ(set.Bindings()[runId].m_FrameDuration, 0.05f);
    EXPECT_EQ(set.Bindings()[runId].m_Clip->VirtualPath(), "anim/run.toml");
    EXPECT_EQ(set.ClipPaths()[runId], "anim/run.toml");
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_StateClipIdIndexesBindings)
{
    auto set = LoadSet(kBothClips);

    asge::game::asset::AssetManager mgr(m_Vfs);
    ASSERT_TRUE(set.ResolveAssets(mgr).IsOk());

    auto const& machine = set.Machine();
    auto const runState = machine.FindState("run").Value();
    auto const clipId = machine.States()[runState].m_ClipId;
    EXPECT_EQ(set.Bindings()[clipId].m_Clip->VirtualPath(), "anim/run.toml");
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_CanRunTwice)
{
    auto set = LoadSet(kBothClips);

    asge::game::asset::AssetManager mgr(m_Vfs);
    ASSERT_TRUE(set.ResolveAssets(mgr).IsOk());
    ASSERT_TRUE(set.ResolveAssets(mgr).IsOk());
    EXPECT_EQ(set.Bindings().size(), 2u);
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_SetsShareTheCachedMachine)
{
    auto first = LoadSet(kBothClips);
    auto second = LoadSet(kBothClips);

    asge::game::asset::AssetManager mgr(m_Vfs);
    ASSERT_TRUE(first.ResolveAssets(mgr).IsOk());
    ASSERT_TRUE(second.ResolveAssets(mgr).IsOk());
    EXPECT_EQ(&first.Machine(), &second.Machine());
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_MissingMachineReturnsError)
{
    auto set = LoadSet(kBothClips, "anim/ghost.animsm");

    asge::game::asset::AssetManager mgr(m_Vfs);
    EXPECT_FALSE(set.ResolveAssets(mgr).IsOk());
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_ClipUnknownToMachineReturnsClipNotFound)
{
    auto set = LoadSet(std::string(kBothClips) + "<Clip name=\"fly\" path=\"anim/idle.toml\"/>\n");

    asge::game::asset::AssetManager mgr(m_Vfs);
    auto result = set.ResolveAssets(mgr);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::ClipNotFound));
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_UnboundMachineClipReturnsClipNotFound)
{
    auto set = LoadSet("<Clip name=\"idle\" path=\"anim/idle.toml\"/>\n");

    asge::game::asset::AssetManager mgr(m_Vfs);
    auto result = set.ResolveAssets(mgr);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(AnimStateMachineErrors::ClipNotFound));
}

TEST_F(AnimationClipSetResolveTest, ResolveAssets_MissingFrameTableReturnsErrorAndChangesNothing)
{
    auto set = LoadSet(
        "<Clip name=\"run\" path=\"anim/run.toml\"/>\n<Clip name=\"idle\" path=\"anim/ghost.toml\"/>\n");

    asge::game::asset::AssetManager mgr(m_Vfs);
    ASSERT_FALSE(set.ResolveAssets(mgr).IsOk());

    // Still in declaration order, nothing resolved
    EXPECT_EQ(set.ClipNames()[0], "run");
    EXPECT_EQ(set.Bindings()[0].m_Clip, nullptr);
}

} // namespace

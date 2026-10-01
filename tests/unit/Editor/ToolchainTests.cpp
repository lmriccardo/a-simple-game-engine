#include "Toolchain.hpp"

#include <gtest/gtest.h>

namespace
{

// ─── ParseVsWhere ────────────────────────────────────────────────────────────

TEST(ParseVsWhereTest, ReadsEveryInstallationBlock)
{
    auto const installs = ParseVsWhere(
        "instanceId: 1a2b\r\n"
        "installDate: 5/1/2026\r\n"
        "installationName: VisualStudio/18.5.4\r\n"
        "installationVersion: 18.5.11612.150\r\n"
        "displayName: Visual Studio Community 2026\r\n"
        "\r\n"
        "instanceId: 9f8e\r\n"
        "installationVersion: 17.9.34728.123\r\n"
        "displayName: Visual Studio Build Tools 2022\r\n");

    ASSERT_EQ(installs.size(), 2u);
    EXPECT_EQ(installs[0].m_DisplayName, "Visual Studio Community 2026");
    EXPECT_EQ(installs[0].m_Version, "18.5.11612.150");
    EXPECT_EQ(installs[1].m_DisplayName, "Visual Studio Build Tools 2022");
    EXPECT_EQ(installs[1].m_Version, "17.9.34728.123");
}

TEST(ParseVsWhereTest, EmptyOutput_ReturnsNothing)
{
    EXPECT_TRUE(ParseVsWhere("").empty());
}

TEST(ParseVsWhereTest, BlockWithoutAVersion_IsDropped)
{
    EXPECT_TRUE(ParseVsWhere("instanceId: 1\ndisplayName: Broken\n").empty());
}

// ─── VsGenerator ─────────────────────────────────────────────────────────────

TEST(VsGeneratorTest, KnownMajorVersions_MapToCMakeGenerators)
{
    EXPECT_EQ(VsGenerator("18.5.11612.150"), "Visual Studio 18 2026");
    EXPECT_EQ(VsGenerator("17.9.34728.123"), "Visual Studio 17 2022");
    EXPECT_EQ(VsGenerator("16.11.0"), "Visual Studio 16 2019");
}

TEST(VsGeneratorTest, UnknownVersion_ReturnsNullopt)
{
    EXPECT_FALSE(VsGenerator("15.9.0").has_value());
    EXPECT_FALSE(VsGenerator("garbage").has_value());
}

// ─── CompilerMajorVersion ────────────────────────────────────────────────────

TEST(CompilerMajorVersionTest, Gcc_ReadsTheTrailingVersion)
{
    EXPECT_EQ(CompilerMajorVersion("g++ (Ubuntu 13.2.0-23ubuntu4) 13.2.0"), 13);
}

TEST(CompilerMajorVersionTest, Clang_ReadsTheWordAfterVersion)
{
    EXPECT_EQ(CompilerMajorVersion("clang version 17.0.6"), 17);
    EXPECT_EQ(CompilerMajorVersion("Apple clang version 15.0.0 (clang-1500.3.9.4)"), 15);
}

TEST(CompilerMajorVersionTest, Garbage_ReturnsNullopt)
{
    EXPECT_FALSE(CompilerMajorVersion("").has_value());
    EXPECT_FALSE(CompilerMajorVersion("no numbers here").has_value());
}

// ─── ScanToolchains ──────────────────────────────────────────────────────────

TEST(ScanToolchainsTest, EveryEntryHasAnIdAConfigureArgumentListAndAReasonWhenUnusable)
{
    for (auto const& toolchain : ScanToolchains())
    {
        EXPECT_FALSE(toolchain.m_Id.empty());
        EXPECT_FALSE(toolchain.m_Name.empty());
        EXPECT_FALSE(toolchain.m_ConfigureArgs.empty());
        if (!toolchain.m_Usable) EXPECT_FALSE(toolchain.m_Reason.empty());
    }
}

}

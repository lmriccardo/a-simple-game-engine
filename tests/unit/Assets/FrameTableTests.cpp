#include <ASGE/Game/Assets/FrameTable.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using namespace asge::game::asset;

// ─── FrameTable::Load ────────────────────────────────────────────────────────

class FrameTableLoadTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Path;

    void SetUp() override
    {
        auto const uniqueName = "asge_frametable_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".toml";
        m_Path = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_Path, ec);
    }

    void Write(std::string const& inContent) const
    {
        std::ofstream file(m_Path, std::ios::trunc);
        file << inContent;
    }
};

TEST_F(FrameTableLoadTest, ValidTableBuildsExpectedFrameGrid)
{
    Write(
        "[FrameTable]\n"
        "x = 0.0\n"
        "y = 0.0\n"
        "w = 8.0\n"
        "h = 8.0\n"
        "columns = 2\n"
        "count = 4\n"
    );

    auto result = FrameTable::Load(m_Path);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().m_Frames.size(), 4u);
    EXPECT_FLOAT_EQ(result.Value().m_Frames[0].x, 0.0f); // col 0, row 0
    EXPECT_FLOAT_EQ(result.Value().m_Frames[1].x, 8.0f); // col 1, row 0
    EXPECT_FLOAT_EQ(result.Value().m_Frames[2].x, 0.0f); // wraps to col 0, row 1
    EXPECT_FLOAT_EQ(result.Value().m_Frames[2].y, 8.0f);
}

TEST_F(FrameTableLoadTest, OffsetCellShiftsEveryFrame)
{
    Write(
        "[FrameTable]\n"
        "x = 100.0\n"
        "y = 50.0\n"
        "w = 4.0\n"
        "h = 4.0\n"
        "columns = 2\n"
        "count = 2\n"
    );

    auto result = FrameTable::Load(m_Path);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().m_Frames.size(), 2u);
    EXPECT_FLOAT_EQ(result.Value().m_Frames[0].x, 100.0f);
    EXPECT_FLOAT_EQ(result.Value().m_Frames[0].y, 50.0f);
    EXPECT_FLOAT_EQ(result.Value().m_Frames[1].x, 104.0f);
}

TEST_F(FrameTableLoadTest, MissingFrameTableSectionDefaultsToEmptyFrameListRatherThanError)
{
    // TOMLTableView::Table() auto-vivifies a missing table with every field
    // defaulted, matching every other Serializer<T>::FromToml in this
    // codebase -- a file with no "[FrameTable]" section at all still parses
    // successfully, just with zero frames.
    Write("title = \"not a frame table\"\n");

    auto result = FrameTable::Load(m_Path);
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().m_Frames.empty());
}

TEST_F(FrameTableLoadTest, NonExistentPathReturnsError)
{
    auto result = FrameTable::Load(m_Path); // Never written by this test
    EXPECT_FALSE(result.IsOk());
}

TEST_F(FrameTableLoadTest, MalformedTomlReturnsError)
{
    Write("not a valid line\n");

    auto result = FrameTable::Load(m_Path);
    EXPECT_FALSE(result.IsOk());
}

// ─── MakeGridFrames ─────────────────────────────────────────────────────────

TEST(MakeGridFramesTest, LaysOutFramesInRowMajorOrderAcrossColumns)
{
    auto const frames = MakeGridFrames( asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, 3, 4 );

    ASSERT_EQ(frames.size(), 4u);
    EXPECT_FLOAT_EQ(frames[0].x, 0.0f);  EXPECT_FLOAT_EQ(frames[0].y, 0.0f); // col 0, row 0
    EXPECT_FLOAT_EQ(frames[1].x, 8.0f);  EXPECT_FLOAT_EQ(frames[1].y, 0.0f); // col 1, row 0
    EXPECT_FLOAT_EQ(frames[2].x, 16.0f); EXPECT_FLOAT_EQ(frames[2].y, 0.0f); // col 2, row 0
    EXPECT_FLOAT_EQ(frames[3].x, 0.0f);  EXPECT_FLOAT_EQ(frames[3].y, 8.0f); // wraps to col 0, row 1
    for ( auto const& frame : frames )
    {
        EXPECT_FLOAT_EQ(frame.w, 8.0f);
        EXPECT_FLOAT_EQ(frame.h, 8.0f);
    }
}

TEST(MakeGridFramesTest, OffsetSheetCellShiftsEveryFrameByThatOffset)
{
    auto const frames = MakeGridFrames( asge::math::Rect{ 100.0f, 50.0f, 4.0f, 4.0f }, 2, 2 );

    ASSERT_EQ(frames.size(), 2u);
    EXPECT_FLOAT_EQ(frames[0].x, 100.0f);
    EXPECT_FLOAT_EQ(frames[0].y, 50.0f);
    EXPECT_FLOAT_EQ(frames[1].x, 104.0f);
    EXPECT_FLOAT_EQ(frames[1].y, 50.0f);
}

TEST(MakeGridFramesTest, CountSmallerThanGridSizeOnlyProducesThatManyFrames)
{
    // A 4-column, 2-row sheet (8 cells), but only 6 are actual animation frames.
    auto const frames = MakeGridFrames( asge::math::Rect{ 0.0f, 0.0f, 10.0f, 10.0f }, 4, 6 );
    EXPECT_EQ(frames.size(), 6u);
}

TEST(MakeGridFramesTest, ZeroCountReturnsEmpty)
{
    EXPECT_TRUE(MakeGridFrames( asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, 4, 0 ).empty());
}

TEST(MakeGridFramesTest, ZeroColumnsReturnsEmptyRatherThanDividingByZero)
{
    EXPECT_TRUE(MakeGridFrames( asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, 0, 4 ).empty());
}

}

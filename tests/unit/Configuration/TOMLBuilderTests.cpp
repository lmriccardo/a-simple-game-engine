#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>
#include <ASGE/Core/Configuration/ConfigurationManager.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using namespace asge::config;

// ─── Set / SetArray on the root table ──────────────────────────────────────────

TEST(TOMLBuilderTest, Set_NewIntKeyAppearsInOutput)
{
    TOMLBuilder builder;
    builder.Set<int>("count", 42);
    EXPECT_NE(builder.ToString().find("count = 42"), std::string::npos);
}

TEST(TOMLBuilderTest, Set_NewStringKeySerializesAsBasicString)
{
    TOMLBuilder builder;
    builder.Set<std::string>("name", "Alice");
    EXPECT_NE(builder.ToString().find(R"(name = "Alice")"), std::string::npos);
}

TEST(TOMLBuilderTest, Set_SameKeyTwiceOverwritesValue)
{
    TOMLBuilder builder;
    builder.Set<int>("count", 1);
    builder.Set<int>("count", 2);
    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("count = 2"), std::string::npos);
    EXPECT_EQ(dump.find("count = 1"), std::string::npos);
}

TEST(TOMLBuilderTest, Set_ChainsAcrossMultipleKeys)
{
    TOMLBuilder builder;
    builder.Set<int>("count", 1).Set<bool>("enabled", true);
    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("count = 1"), std::string::npos);
    EXPECT_NE(dump.find("enabled = true"), std::string::npos);
}

TEST(TOMLBuilderTest, SetArray_NewKeyAppearsInOutput)
{
    TOMLBuilder builder;
    builder.SetArray<int>("nums", {1, 2, 3});
    EXPECT_NE(builder.ToString().find("nums = [1, 2, 3]"), std::string::npos);
}

TEST(TOMLBuilderTest, SetArray_NestedArraySerializesCorrectly)
{
    TOMLBuilder builder;
    std::vector<std::vector<int>> matrix{ {1, 2}, {3, 4} };
    builder.SetArray<std::vector<int>>("matrix", matrix);
    EXPECT_NE(builder.ToString().find("matrix = [[1, 2], [3, 4]]"), std::string::npos);
}

// ─── Table() — subtable scoping ────────────────────────────────────────────────

TEST(TOMLBuilderTest, Table_CreatesHeaderAndScopesKeys)
{
    TOMLBuilder builder;
    builder.Table("server").Set<int>("port", 8080);
    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[server]"), std::string::npos);
    EXPECT_NE(dump.find("port = 8080"), std::string::npos);
}

TEST(TOMLBuilderTest, Table_DottedPathCreatesNestedTables)
{
    TOMLBuilder builder;
    builder.Table("a.b").Set<int>("value", 1);
    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[a.b]"), std::string::npos);
    EXPECT_NE(dump.find("value = 1"), std::string::npos);
}

TEST(TOMLBuilderTest, Table_CalledTwiceReturnsSameUnderlyingTable)
{
    TOMLBuilder builder;
    builder.Table("server").Set<int>("port", 8080);
    builder.Table("server").Set<std::string>("host", "localhost");

    auto const dump = builder.ToString();
    EXPECT_EQ(dump.find("[server]"), dump.rfind("[server]")); // header appears once
    EXPECT_NE(dump.find("port = 8080"), std::string::npos);
    EXPECT_NE(dump.find(R"(host = "localhost")"), std::string::npos);
}

// ─── Round-trip through the parser ─────────────────────────────────────────────

TEST(TOMLBuilderTest, ToString_ReparsesToTheSameValues)
{
    TOMLBuilder builder;
    builder.Set<std::string>("title", "My Config").Set<int>("version", 1);
    builder.Table("server").Set<int>("port", 8080).SetArray<std::string>("tags", {"prod", "eu"});

    auto parsed = _internal::toml::Parse(builder.ToString());
    ASSERT_TRUE(parsed.IsOk());

    auto root = parsed.Value();
    EXPECT_EQ(*root->Get<std::string>("title").Value(), "My Config");
    EXPECT_EQ(*root->Get<int>("version").Value(), 1);
    EXPECT_EQ(*root->Get<int>("server.port").Value(), 8080);

    auto tags = root->GetTypedArray<std::string>("server.tags");
    ASSERT_TRUE(tags.IsOk());
    ASSERT_EQ(tags.Value().size(), 2u);
    EXPECT_EQ(tags.Value()[0], "prod");
    EXPECT_EQ(tags.Value()[1], "eu");
}

// ─── SaveToFile ─────────────────────────────────────────────────────────────────

class TOMLBuilderSaveTest : public ::testing::Test
{
protected:
    std::filesystem::path m_TomlPath;

    void SetUp() override
    {
        auto const uniqueName = "asge_toml_builder_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".toml";
        m_TomlPath = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_TomlPath, ec);
    }
};

TEST_F(TOMLBuilderSaveTest, SaveToFile_WritesReadableToml)
{
    TOMLBuilder builder;
    builder.Set<int>("count", 7);
    builder.Table("server").Set<int>("port", 9090);

    ASSERT_TRUE(builder.SaveToFile(m_TomlPath).IsOk());

    std::ifstream file(m_TomlPath);
    std::ostringstream oss;
    oss << file.rdbuf();
    auto const savedText = oss.str();

    EXPECT_NE(savedText.find("count = 7"), std::string::npos);
    EXPECT_NE(savedText.find("[server]"), std::string::npos);
    EXPECT_NE(savedText.find("port = 9090"), std::string::npos);
}

TEST_F(TOMLBuilderSaveTest, SaveToFile_RoundTripsThroughConfigurationManager)
{
    TOMLBuilder builder;
    builder.Set<std::string>("name", "demo").Set<int>("count", 3);
    ASSERT_TRUE(builder.SaveToFile(m_TomlPath).IsOk());

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    EXPECT_EQ(cm.Get<std::string>("name").Value(), "demo");
    EXPECT_EQ(cm.Get<int>("count").Value(), 3);
}

}

#include <ASGE/Core/Formats/TOML/TOML_Parser.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using namespace asge::formats::toml;
using namespace asge::formats::toml::_internal;
using asge::errors::ConfError;

// ─── Result<T> helpers ────────────────────────────────────────────────────────
// The parser API returns asge::Result<T> everywhere. These helpers collapse
// the happy path down to the raw value (or nullptr/empty on error) so the
// bulk of the tests below can stay focused on parsing behaviour; the
// dedicated "error case" tests further down inspect the Result/error code
// directly.

table_pointer ParseOk(std::string const& inRaw)
{
    auto result = Parse(inRaw);
    EXPECT_TRUE(result.IsOk());
    return result.IsOk() ? result.Value() : nullptr;
}

template<typename T>
T const* GetOrNull(table_pointer const& inTable, std::string const& inPath)
{
    auto result = inTable->Get<T>(inPath);
    return result.IsOk() ? result.Value() : nullptr;
}

table_pointer GetTableOrNull(table_pointer const& inTable, std::string const& inPath)
{
    auto result = inTable->GetTable(inPath);
    return result.IsOk() ? result.Value() : nullptr;
}

std::string DumpTable(table_pointer const& inTable)
{
    std::ostringstream oss;
    oss << *inTable;
    return oss.str();
}

template<typename T>
std::vector<T> GetArrayOrEmpty(table_pointer const& inTable, std::string const& inPath)
{
    auto result = inTable->template GetTypedArray<T>(inPath);
    return result.IsOk() ? result.Value() : std::vector<T>{};
}

// ─── Parse — root scalars ─────────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_IntValue)
{
    auto root = ParseOk("count = 7\n");
    auto* val = GetOrNull<int>(root, "count");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 7);
}

TEST(TOMLParserTest, Parse_IntPastInt32RangeWidensToInt64)
{
    // Past INT_MAX -- std::stoi throws out_of_range, which ParseValue must
    // catch and retry as an Int64 rather than failing the whole parse.
    auto root = ParseOk("big = 4294967296\n");
    EXPECT_EQ(GetOrNull<int>(root, "big"), nullptr); // not stored as a 32-bit int...
    auto* val = GetOrNull<std::int64_t>(root, "big");
    ASSERT_NE(val, nullptr); // ...but as an Int64
    EXPECT_EQ(*val, 4294967296LL);
}

TEST(TOMLParserTest, Parse_SmallIntStillParsesAsPlainIntNotInt64)
{
    // Regression: adding Int64 support must not widen every ordinary int.
    auto root = ParseOk("count = 42\n");
    EXPECT_EQ(GetOrNull<std::int64_t>(root, "count"), nullptr);
    auto* val = GetOrNull<int>(root, "count");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 42);
}

TEST(TOMLParserTest, Parse_DoubleValue)
{
    auto root = ParseOk("pi = 3.14\n");
    auto* val = GetOrNull<double>(root, "pi");
    ASSERT_NE(val, nullptr);
    EXPECT_DOUBLE_EQ(*val, 3.14);
}

TEST(TOMLParserTest, Parse_BoolTrue)
{
    auto root = ParseOk("flag = true\n");
    auto* val = GetOrNull<bool>(root, "flag");
    ASSERT_NE(val, nullptr);
    EXPECT_TRUE(*val);
}

TEST(TOMLParserTest, Parse_BoolFalse)
{
    auto root = ParseOk("flag = false\n");
    auto* val = GetOrNull<bool>(root, "flag");
    ASSERT_NE(val, nullptr);
    EXPECT_FALSE(*val);
}

TEST(TOMLParserTest, Parse_StringValue)
{
    auto root = ParseOk(R"(name = "Alice")" "\n");
    auto* val = GetOrNull<std::string>(root, "name");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "Alice");
}

TEST(TOMLParserTest, Parse_MultipleRootKeys)
{
    auto root = ParseOk("a = 1\nb = 2\n");
    EXPECT_EQ(*GetOrNull<int>(root, "a"), 1);
    EXPECT_EQ(*GetOrNull<int>(root, "b"), 2);
}

TEST(TOMLParserTest, Parse_CommentLinesIgnored)
{
    auto root = ParseOk("# this is a comment\nkey = 99\n");
    auto* val = GetOrNull<int>(root, "key");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 99);
}

TEST(TOMLParserTest, Parse_InlineCommentStripped)
{
    auto root = ParseOk("key = 10 # inline comment\n");
    auto* val = GetOrNull<int>(root, "key");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 10);
}

// ─── Parse — strings ─────────────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_EmptyBasicString)
{
    auto result = Parse(std::string(R"(a = "")"));
    ASSERT_TRUE(result.IsOk());
    auto value = result.Value()->Get<std::string>("a");
    ASSERT_TRUE(value.IsOk());
    EXPECT_TRUE(value.Value()->empty());
}

TEST(TOMLParserTest, Parse_EmptyLiteralString)
{
    auto result = Parse(std::string("a = ''"));
    ASSERT_TRUE(result.IsOk());
    auto value = result.Value()->Get<std::string>("a");
    ASSERT_TRUE(value.IsOk());
    EXPECT_TRUE(value.Value()->empty());
}

TEST(TOMLParserTest, Parse_EmptyStringRoundTrip)
{
    auto root = ParseOk("[sprite]\nname = 'hero'\nvirtual_path = \"placeholder\"\n");
    ASSERT_NE(root, nullptr);
    ASSERT_TRUE(root->Set<std::string>("sprite.virtual_path", "").IsOk());

    auto result = Parse(DumpTable(root));
    ASSERT_TRUE(result.IsOk());
    auto path = result.Value()->Get<std::string>("sprite.virtual_path");
    ASSERT_TRUE(path.IsOk());
    EXPECT_TRUE(path.Value()->empty());
    auto name = result.Value()->Get<std::string>("sprite.name");
    ASSERT_TRUE(name.IsOk());
    EXPECT_EQ(*name.Value(), "hero");
}

TEST(TOMLParserTest, Parse_InvalidStringStillReturnsError)
{
    for ( auto const* input : { "a = \"", "a = '", R"(a = "\q")" } )
    {
        SCOPED_TRACE(input);
        auto result = Parse(std::string(input));
        ASSERT_FALSE(result.IsOk());
        EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlInvalidString));
    }
}

TEST(TOMLParserTest, Parse_BasicStringEscape)
{
    auto root = ParseOk(R"(msg = "hello\nworld")" "\n");
    auto* val = GetOrNull<std::string>(root, "msg");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "hello\nworld");
}

TEST(TOMLParserTest, Parse_LiteralStringNoEscape)
{
    auto root = ParseOk("msg = 'no\\nescape'\n");
    auto* val = GetOrNull<std::string>(root, "msg");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "no\\nescape");
}

// ─── Parse — arrays ───────────────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_IntArray)
{
    auto root = ParseOk("nums = [1, 2, 3]\n");
    auto vec  = GetArrayOrEmpty<int>(root, "nums");
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
}

TEST(TOMLParserTest, Parse_StringArray)
{
    auto root = ParseOk(R"(tags = ["a", "b", "c"])" "\n");
    auto vec  = GetArrayOrEmpty<std::string>(root, "tags");
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], "a");
    EXPECT_EQ(vec[1], "b");
    EXPECT_EQ(vec[2], "c");
}

TEST(TOMLParserTest, Parse_DoubleArray)
{
    auto root = ParseOk("vals = [1.1, 2.2]\n");
    auto vec  = GetArrayOrEmpty<double>(root, "vals");
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_DOUBLE_EQ(vec[0], 1.1);
    EXPECT_DOUBLE_EQ(vec[1], 2.2);
}

TEST(TOMLParserTest, Parse_EmptyArray)
{
    auto root = ParseOk("empty = []\n");
    auto vec  = GetArrayOrEmpty<int>(root, "empty");
    EXPECT_TRUE(vec.empty());
}

// ─── Parse — standard tables ──────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_StandardTableAccessViaGetTable)
{
    auto root = ParseOk("[server]\nport = 8080\n");
    auto tbl  = GetTableOrNull(root, "server");
    ASSERT_NE(tbl, nullptr);
    auto* val = GetOrNull<int>(tbl, "port");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 8080);
}

TEST(TOMLParserTest, Parse_StandardTableAccessViaDottedPath)
{
    auto root = ParseOk("[server]\nport = 8080\n");
    auto* val = GetOrNull<int>(root, "server.port");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 8080);
}

TEST(TOMLParserTest, Parse_NestedTableDottedPath)
{
    auto root = ParseOk("[server.tls]\nenabled = true\n");
    auto* val = GetOrNull<bool>(root, "server.tls.enabled");
    ASSERT_NE(val, nullptr);
    EXPECT_TRUE(*val);
}

TEST(TOMLParserTest, Parse_RootAndTableKeysCoexist)
{
    auto root = ParseOk("root_key = 1\n[section]\nkey = 2\n");
    EXPECT_EQ(*GetOrNull<int>(root, "root_key"), 1);
    EXPECT_EQ(*GetOrNull<int>(root, "section.key"), 2);
}

TEST(TOMLParserTest, Parse_MultipleStandardTables)
{
    auto root = ParseOk("[a]\nval = 1\n[b]\nval = 2\n");
    EXPECT_EQ(*GetOrNull<int>(root, "a.val"), 1);
    EXPECT_EQ(*GetOrNull<int>(root, "b.val"), 2);
}

// ─── Parse — array tables ─────────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_SingleArrayTableEntry)
{
    auto root = ParseOk("[[items]]\nname = \"first\"\n");
    auto tbl  = GetTableOrNull(root, "items[0]");
    ASSERT_NE(tbl, nullptr);
    auto* val = GetOrNull<std::string>(tbl, "name");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "first");
}

TEST(TOMLParserTest, Parse_MultipleArrayTableEntries)
{
    auto root = ParseOk(
        "[[products]]\n"
        "name = \"Hammer\"\n"
        "sku  = 1\n"
        "[[products]]\n"
        "name = \"Nail\"\n"
        "sku  = 2\n"
    );

    auto t0 = GetTableOrNull(root, "products[0]");
    auto t1 = GetTableOrNull(root, "products[1]");
    ASSERT_NE(t0, nullptr);
    ASSERT_NE(t1, nullptr);
    EXPECT_EQ(*GetOrNull<std::string>(t0, "name"), "Hammer");
    EXPECT_EQ(*GetOrNull<std::string>(t1, "name"), "Nail");
    EXPECT_EQ(*GetOrNull<int>(t0, "sku"), 1);
    EXPECT_EQ(*GetOrNull<int>(t1, "sku"), 2);
}

TEST(TOMLParserTest, Parse_ArrayTableElementsScopeTheirOwnNestedTable)
{
    // A bare [entity.Transform] header after a run of [[entity]] blocks must
    // attach to the most recently opened entity, not always the first one.
    auto root = ParseOk(
        "[[entity]]\n"
        "id = 0\n"
        "[entity.Transform]\n"
        "x = 0.0\n"
        "y = 0.0\n"
        "[[entity]]\n"
        "id = 1\n"
        "[entity.Transform]\n"
        "x = 1.0\n"
        "y = 2.0\n"
    );

    ASSERT_EQ(root->GetSubTables().size(), 2u);

    auto t0 = GetTableOrNull(root, "entity[0]");
    auto t1 = GetTableOrNull(root, "entity[1]");
    ASSERT_NE(t0, nullptr);
    ASSERT_NE(t1, nullptr);

    EXPECT_EQ(*GetOrNull<int>(t0, "id"), 0);
    EXPECT_EQ(*GetOrNull<double>(t0, "Transform.x"), 0.0);
    EXPECT_EQ(*GetOrNull<double>(t0, "Transform.y"), 0.0);

    EXPECT_EQ(*GetOrNull<int>(t1, "id"), 1);
    EXPECT_EQ(*GetOrNull<double>(t1, "Transform.x"), 1.0);
    EXPECT_EQ(*GetOrNull<double>(t1, "Transform.y"), 2.0);
}

// ─── Parse — malformed input ──────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_UnexpectedTokenReturnsError)
{
    auto result = Parse(std::string("not a valid line\n"));
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlUnexpectedToken));
}

TEST(TOMLParserTest, Parse_DuplicateKeyReturnsError)
{
    auto result = Parse(std::string("key = 1\nkey = 2\n"));
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlAlreadyExistingKey));
}

// ─── Table::Get — error cases ─────────────────────────────────────────────────

TEST(TOMLParserTest, Get_MissingKeyReturnsError)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->Get<int>("missing");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoAttribute));
}

TEST(TOMLParserTest, Get_WrongTypeReturnsError)
{
    auto root   = ParseOk("key = 42\n");
    auto result = root->Get<std::string>("key");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlTypeMismatch));
}

TEST(TOMLParserTest, Get_MissingTableInPathReturnsError)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->Get<int>("no_table.key");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoSubtable));
}

// ─── Table::GetTable — error cases ───────────────────────────────────────────

TEST(TOMLParserTest, GetTable_MissingTableReturnsError)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->GetTable("nonexistent");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoSubtable));
}

TEST(TOMLParserTest, GetTable_CorrectNameReturnsTable)
{
    auto root = ParseOk("[section]\nval = 5\n");
    EXPECT_NE(GetTableOrNull(root, "section"), nullptr);
}

TEST(TOMLParserTest, GetTable_NameMatchesHeader)
{
    auto root = ParseOk("[section]\nval = 5\n");
    auto tbl  = GetTableOrNull(root, "section");
    ASSERT_NE(tbl, nullptr);
    EXPECT_EQ(tbl->GetName(), "section");
}

// ─── Table::GetTypedArray — error cases ───────────────────────────────────────

TEST(TOMLParserTest, GetTypedArray_MissingKeyReturnsEmpty)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->GetTypedArray<int>("missing");
    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().empty());
}

// ─── Table::Set — success cases ───────────────────────────────────────────────

TEST(TOMLParserTest, Set_UpdatesIntValue)
{
    auto root = ParseOk("count = 7\n");
    ASSERT_TRUE(root->Set<int>("count", 9).IsOk());
    EXPECT_EQ(*GetOrNull<int>(root, "count"), 9);
}

TEST(TOMLParserTest, Set_UpdatesDoubleValue_KeepsFloatFormatting)
{
    auto root = ParseOk("pi = 3.14\n");
    ASSERT_TRUE(root->Set<double>("pi", 3.0).IsOk());
    EXPECT_DOUBLE_EQ(*GetOrNull<double>(root, "pi"), 3.0);
    EXPECT_NE(DumpTable(root).find("pi = 3.0"), std::string::npos);
}

TEST(TOMLParserTest, Set_UpdatesBoolValue)
{
    auto root = ParseOk("flag = false\n");
    ASSERT_TRUE(root->Set<bool>("flag", true).IsOk());
    EXPECT_TRUE(*GetOrNull<bool>(root, "flag"));
}

TEST(TOMLParserTest, Set_UpdatesBasicString_KeepsBasicSubtype)
{
    auto root = ParseOk(R"(name = "Alice")" "\n");
    ASSERT_TRUE(root->Set<std::string>("name", "Bob").IsOk());
    EXPECT_EQ(*GetOrNull<std::string>(root, "name"), "Bob");
    EXPECT_NE(DumpTable(root).find(R"(name = "Bob")"), std::string::npos);
}

TEST(TOMLParserTest, Set_UpdatesLiteralString_KeepsLiteralSubtype)
{
    auto root = ParseOk("msg = 'no\\nescape'\n");
    ASSERT_TRUE(root->Set<std::string>("msg", "new\\nvalue").IsOk());
    EXPECT_NE(DumpTable(root).find("msg = 'new\\nvalue'"), std::string::npos);
}

TEST(TOMLParserTest, Set_UpdatesMultiBasicString_KeepsMultiBasicSubtype)
{
    auto root = ParseOk("msg = \"\"\"hello\nworld\"\"\"\n");
    ASSERT_TRUE(root->Set<std::string>("msg", "updated").IsOk());
    EXPECT_NE(DumpTable(root).find(R"(msg = """updated""")"), std::string::npos);
}

TEST(TOMLParserTest, Set_UpdatesMultiLiteralString_KeepsMultiLiteralSubtype)
{
    auto root = ParseOk("msg = '''hello\nworld'''\n");
    ASSERT_TRUE(root->Set<std::string>("msg", "updated").IsOk());
    EXPECT_NE(DumpTable(root).find("msg = '''updated'''"), std::string::npos);
}

TEST(TOMLParserTest, Set_ThroughDottedPathUpdatesNestedTable)
{
    auto root = ParseOk("[server]\nport = 8080\n");
    ASSERT_TRUE(root->Set<int>("server.port", 9090).IsOk());
    EXPECT_EQ(*GetOrNull<int>(root, "server.port"), 9090);
}

// ─── Table::Set — error cases ─────────────────────────────────────────────────

TEST(TOMLParserTest, Set_FailsOnMissingKey)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->Set<int>("missing", 2);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoAttribute));
}

TEST(TOMLParserTest, Set_FailsOnMissingParentTable)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->Set<int>("no_table.key", 2);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoSubtable));
}

TEST(TOMLParserTest, Set_FailsOnTypeMismatch)
{
    auto root   = ParseOk("key = 42\n");
    auto result = root->Set<std::string>("key", "oops");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlTypeMismatch));
}

// ─── Table::SetTypedArray ─────────────────────────────────────────────────────

TEST(TOMLParserTest, SetTypedArray_UpdatesIntArray)
{
    auto root = ParseOk("nums = [1, 2, 3]\n");
    ASSERT_TRUE(root->SetTypedArray<int>("nums", std::vector<int>{4, 5}).IsOk());
    auto vec = GetArrayOrEmpty<int>(root, "nums");
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], 4);
    EXPECT_EQ(vec[1], 5);
}

TEST(TOMLParserTest, SetTypedArray_UpdatesNestedArray)
{
    auto root = ParseOk("matrix = [[1, 2], [3, 4]]\n");
    std::vector<std::vector<int>> newMatrix{ {5, 6}, {7, 8}, {9, 10} };
    ASSERT_TRUE(root->SetTypedArray<std::vector<int>>("matrix", newMatrix).IsOk());
    EXPECT_NE(DumpTable(root).find("matrix = [[5, 6], [7, 8], [9, 10]]"), std::string::npos);
}

TEST(TOMLParserTest, SetTypedArray_EmptyExistingArrayFallsBackToDefaultTypeInfo)
{
    auto root = ParseOk("nums = []\n");
    ASSERT_TRUE(root->SetTypedArray<int>("nums", std::vector<int>{1, 2}).IsOk());
    EXPECT_NE(DumpTable(root).find("nums = [1, 2]"), std::string::npos);
}

TEST(TOMLParserTest, SetTypedArray_FailsOnMissingKey)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->SetTypedArray<int>("missing", std::vector<int>{1});
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoAttribute));
}

TEST(TOMLParserTest, SetTypedArray_FailsOnTypeMismatch)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->SetTypedArray<int>("key", std::vector<int>{1});
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlTypeMismatch));
}

// ─── Table::SetOrCreate ────────────────────────────────────────────────────────

TEST(TOMLParserTest, SetOrCreate_CreatesMissingKey)
{
    auto root = ParseOk("key = 1\n");
    ASSERT_TRUE(root->SetOrCreate<int>("count", 42).IsOk());
    EXPECT_EQ(*GetOrNull<int>(root, "count"), 42);
}

TEST(TOMLParserTest, SetOrCreate_OverwritesExistingKey)
{
    auto root = ParseOk("count = 1\n");
    ASSERT_TRUE(root->SetOrCreate<int>("count", 2).IsOk());
    EXPECT_EQ(*GetOrNull<int>(root, "count"), 2);
}

TEST(TOMLParserTest, SetOrCreate_NewStringKeySerializesAsBasicString)
{
    auto root = ParseOk("key = 1\n");
    ASSERT_TRUE(root->SetOrCreate<std::string>("name", "Alice").IsOk());
    EXPECT_NE(DumpTable(root).find(R"(name = "Alice")"), std::string::npos);
}

TEST(TOMLParserTest, SetOrCreate_StillFailsOnTypeMismatch)
{
    auto root   = ParseOk("count = 1\n");
    auto result = root->SetOrCreate<std::string>("count", "oops");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlTypeMismatch));
}

TEST(TOMLParserTest, SetOrCreate_StillFailsOnMissingParentTable)
{
    auto root   = ParseOk("key = 1\n");
    auto result = root->SetOrCreate<int>("no_table.key", 2);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoSubtable));
}

// ─── Table::SetOrCreateTypedArray ──────────────────────────────────────────────

TEST(TOMLParserTest, SetOrCreateTypedArray_CreatesMissingArray)
{
    auto root = ParseOk("key = 1\n");
    ASSERT_TRUE(root->SetOrCreateTypedArray<int>("nums", std::vector<int>{1, 2, 3}).IsOk());
    EXPECT_NE(DumpTable(root).find("nums = [1, 2, 3]"), std::string::npos);
}

TEST(TOMLParserTest, SetOrCreateTypedArray_OverwritesExistingArray)
{
    auto root = ParseOk("nums = [1, 2, 3]\n");
    ASSERT_TRUE(root->SetOrCreateTypedArray<int>("nums", std::vector<int>{4, 5}).IsOk());
    auto vec = GetArrayOrEmpty<int>(root, "nums");
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], 4);
    EXPECT_EQ(vec[1], 5);
}

TEST(TOMLParserTest, SetOrCreateTypedArray_CreatesMissingNestedArray)
{
    auto root = ParseOk("key = 1\n");
    std::vector<std::vector<int>> matrix{ {1, 2}, {3, 4} };
    ASSERT_TRUE(root->SetOrCreateTypedArray<std::vector<int>>("matrix", matrix).IsOk());
    EXPECT_NE(DumpTable(root).find("matrix = [[1, 2], [3, 4]]"), std::string::npos);
}

}

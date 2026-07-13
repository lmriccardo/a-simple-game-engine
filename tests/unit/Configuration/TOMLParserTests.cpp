#include <ASGE/Core/Configuration/TOML_Parser.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

namespace
{

using namespace asge::config::_internal::toml;
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

// ─── DetectType ───────────────────────────────────────────────────────────────

TEST(TOMLParserTest, DetectType_EmptyIsNull)
{
    EXPECT_EQ(DetectType(""), ElementType::Null);
}

TEST(TOMLParserTest, DetectType_ArrayBracket)
{
    EXPECT_EQ(DetectType("[1, 2]"), ElementType::Array);
}

TEST(TOMLParserTest, DetectType_DoubleQuoteIsString)
{
    EXPECT_EQ(DetectType(R"("hello")"), ElementType::String);
}

TEST(TOMLParserTest, DetectType_SingleQuoteIsString)
{
    EXPECT_EQ(DetectType("'hello'"), ElementType::String);
}

TEST(TOMLParserTest, DetectType_TrueIsBool)
{
    EXPECT_EQ(DetectType("true"), ElementType::Bool);
}

TEST(TOMLParserTest, DetectType_FalseIsBool)
{
    EXPECT_EQ(DetectType("false"), ElementType::Bool);
}

TEST(TOMLParserTest, DetectType_DecimalPointIsDouble)
{
    EXPECT_EQ(DetectType("3.14"), ElementType::Double);
}

TEST(TOMLParserTest, DetectType_DigitsOnlyIsInt)
{
    EXPECT_EQ(DetectType("42"), ElementType::Int);
}

TEST(TOMLParserTest, DetectType_NegativeIntIsInt)
{
    EXPECT_EQ(DetectType("-7"), ElementType::Int);
}

// ─── DetectStringType ─────────────────────────────────────────────────────────

TEST(TOMLParserTest, DetectStringType_DoubleQuoteIsBasic)
{
    EXPECT_EQ(DetectStringType(R"("hello")"), StringType::Basic);
}

TEST(TOMLParserTest, DetectStringType_SingleQuoteIsLiteral)
{
    EXPECT_EQ(DetectStringType("'hello'"), StringType::Literal);
}

TEST(TOMLParserTest, DetectStringType_TripleDoubleIsMultiBasic)
{
    EXPECT_EQ(DetectStringType(R"("""hello""")"), StringType::MultiBasic);
}

TEST(TOMLParserTest, DetectStringType_TripleSingleIsMultiLiteral)
{
    EXPECT_EQ(DetectStringType("'''hello'''"), StringType::MultiLiteral);
}

TEST(TOMLParserTest, DetectStringType_PlainWordIsInvalid)
{
    EXPECT_EQ(DetectStringType("hello"), StringType::Invalid);
}

// ─── StripComment ─────────────────────────────────────────────────────────────

TEST(TOMLParserTest, StripComment_NoCommentUnchanged)
{
    EXPECT_EQ(StripComment("key = 42"), "key = 42");
}

TEST(TOMLParserTest, StripComment_InlineCommentRemoved)
{
    EXPECT_EQ(StripComment("key = 42 # comment"), "key = 42 ");
}

TEST(TOMLParserTest, StripComment_OnlyCommentBecomesEmpty)
{
    EXPECT_EQ(StripComment("# full comment"), "");
}

TEST(TOMLParserTest, StripComment_HashInsideStringPreserved)
{
    EXPECT_EQ(StripComment(R"("url#anchor")"), R"("url#anchor")");
}

// ─── ProcessEscape ────────────────────────────────────────────────────────────

TEST(TOMLParserTest, ProcessEscape_NewlineSequence)
{
    EXPECT_EQ(ProcessEscape("a\\nb"), "a\nb");
}

TEST(TOMLParserTest, ProcessEscape_TabSequence)
{
    EXPECT_EQ(ProcessEscape("a\\tb"), "a\tb");
}

TEST(TOMLParserTest, ProcessEscape_BackslashSequence)
{
    EXPECT_EQ(ProcessEscape("a\\\\b"), "a\\b");
}

TEST(TOMLParserTest, ProcessEscape_QuoteSequence)
{
    EXPECT_EQ(ProcessEscape("a\\\"b"), "a\"b");
}

TEST(TOMLParserTest, ProcessEscape_NoEscapesUnchanged)
{
    EXPECT_EQ(ProcessEscape("hello world"), "hello world");
}

// ─── ParseBasicString ─────────────────────────────────────────────────────────

TEST(TOMLParserTest, ParseBasicString_StripsQuotes)
{
    EXPECT_EQ(ParseBasicString(R"("hello")"), "hello");
}

TEST(TOMLParserTest, ParseBasicString_ProcessesEscapes)
{
    EXPECT_EQ(ParseBasicString(R"("line1\nline2")"), "line1\nline2");
}

TEST(TOMLParserTest, ParseBasicString_LiteralSkipsEscapes)
{
    EXPECT_EQ(ParseBasicString(R"('no\nnewline')", /*isLiteral=*/true), R"(no\nnewline)");
}

TEST(TOMLParserTest, ParseBasicString_EmptyString)
{
    EXPECT_EQ(ParseBasicString(R"("")"), "");
}

// ─── SplitArrayElements ───────────────────────────────────────────────────────

TEST(TOMLParserTest, SplitArrayElements_SingleInt)
{
    auto elems = SplitArrayElements("42");
    ASSERT_EQ(elems.size(), 1u);
    EXPECT_EQ(elems[0], "42");
}

TEST(TOMLParserTest, SplitArrayElements_MultipleInts)
{
    auto elems = SplitArrayElements("1, 2, 3");
    ASSERT_EQ(elems.size(), 3u);
    EXPECT_EQ(elems[0], "1");
    EXPECT_EQ(elems[1], "2");
    EXPECT_EQ(elems[2], "3");
}

TEST(TOMLParserTest, SplitArrayElements_EmptyInput)
{
    EXPECT_TRUE(SplitArrayElements("").empty());
}

TEST(TOMLParserTest, SplitArrayElements_NestedArraysNotSplit)
{
    auto elems = SplitArrayElements("[1, 2], [3, 4]");
    ASSERT_EQ(elems.size(), 2u);
    EXPECT_EQ(elems[0], "[1, 2]");
    EXPECT_EQ(elems[1], "[3, 4]");
}

TEST(TOMLParserTest, SplitArrayElements_CommaInsideStringIgnored)
{
    auto elems = SplitArrayElements(R"("a,b", "c")");
    ASSERT_EQ(elems.size(), 2u);
    EXPECT_EQ(elems[0], R"("a,b")");
    EXPECT_EQ(elems[1], R"("c")");
}

// ─── SplitTablePath ───────────────────────────────────────────────────────────

TEST(TOMLParserTest, SplitTablePath_NoDotReturnsEmptyParent)
{
    auto [parent, child] = SplitTablePath("simple");
    EXPECT_TRUE(parent.empty());
    EXPECT_EQ(child, "simple");
}

TEST(TOMLParserTest, SplitTablePath_OneDotSplitsCorrectly)
{
    auto [parent, child] = SplitTablePath("a.b");
    EXPECT_EQ(parent, "a");
    EXPECT_EQ(child, "b");
}

TEST(TOMLParserTest, SplitTablePath_MultiDotUsesLastDot)
{
    auto [parent, child] = SplitTablePath("a.b.c");
    EXPECT_EQ(parent, "a.b");
    EXPECT_EQ(child, "c");
}

// ─── ParseSegment ─────────────────────────────────────────────────────────────

TEST(TOMLParserTest, ParseSegment_PlainNameHasNoIndex)
{
    auto seg = ParseSegment("name");
    EXPECT_EQ(seg.name, "name");
    EXPECT_FALSE(seg.index.has_value());
}

TEST(TOMLParserTest, ParseSegment_IndexedNameParsesIndex)
{
    auto seg = ParseSegment("items[2]");
    EXPECT_EQ(seg.name, "items");
    ASSERT_TRUE(seg.index.has_value());
    EXPECT_EQ(*seg.index, 2u);
}

TEST(TOMLParserTest, ParseSegment_ZeroIndex)
{
    auto seg = ParseSegment("arr[0]");
    ASSERT_TRUE(seg.index.has_value());
    EXPECT_EQ(*seg.index, 0u);
}

// ─── Parse — root scalars ─────────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_IntValue)
{
    auto root = ParseOk("count = 7\n");
    auto* val = GetOrNull<int>(root, "count");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 7);
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

// ─── Parse — malformed input ──────────────────────────────────────────────────

TEST(TOMLParserTest, Parse_UnexpectedTokenReturnsError)
{
    auto result = Parse("not a valid line\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlUnexpectedToken));
}

TEST(TOMLParserTest, Parse_DuplicateKeyReturnsError)
{
    auto result = Parse("key = 1\nkey = 2\n");
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

// ─── EscapeForOutput ──────────────────────────────────────────────────────────

TEST(TOMLParserTest, EscapeForOutput_EscapesBackslash)
{
    EXPECT_EQ(EscapeForOutput("a\\b", false), "a\\\\b");
}

TEST(TOMLParserTest, EscapeForOutput_EscapesQuote)
{
    EXPECT_EQ(EscapeForOutput(R"(a"b)", false), R"(a\"b)");
}

TEST(TOMLParserTest, EscapeForOutput_SingleLineEscapesNewline)
{
    EXPECT_EQ(EscapeForOutput("a\nb", false), "a\\nb");
}

TEST(TOMLParserTest, EscapeForOutput_MultilineKeepsNewlineRaw)
{
    EXPECT_EQ(EscapeForOutput("a\nb", true), "a\nb");
}

TEST(TOMLParserTest, EscapeForOutput_TabKeptRaw)
{
    EXPECT_EQ(EscapeForOutput("a\tb", false), "a\tb");
}

// ─── WriteValue ───────────────────────────────────────────────────────────────

std::string WriteValueToString(ValueType const& inValue, TOMLTypeInfo const& inInfo)
{
    std::ostringstream oss;
    WriteValue(oss, inValue, inInfo);
    return oss.str();
}

TEST(TOMLParserTest, WriteValue_Int)
{
    EXPECT_EQ(WriteValueToString(ValueType(42), TOMLTypeInfo{ElementType::Int}), "42");
}

TEST(TOMLParserTest, WriteValue_DoubleWholeNumberGetsDecimalPoint)
{
    EXPECT_EQ(WriteValueToString(ValueType(3.0), TOMLTypeInfo{ElementType::Double}), "3.0");
}

TEST(TOMLParserTest, WriteValue_DoubleWithFraction)
{
    EXPECT_EQ(WriteValueToString(ValueType(3.5), TOMLTypeInfo{ElementType::Double}), "3.5");
}

TEST(TOMLParserTest, WriteValue_BoolTrue)
{
    EXPECT_EQ(WriteValueToString(ValueType(true), TOMLTypeInfo{ElementType::Bool}), "true");
}

TEST(TOMLParserTest, WriteValue_BoolFalse)
{
    EXPECT_EQ(WriteValueToString(ValueType(false), TOMLTypeInfo{ElementType::Bool}), "false");
}

TEST(TOMLParserTest, WriteValue_BasicStringEscapes)
{
    TOMLTypeInfo info{ElementType::String, StringType::Basic};
    EXPECT_EQ(WriteValueToString(ValueType(std::string("a\nb")), info), R"("a\nb")");
}

TEST(TOMLParserTest, WriteValue_LiteralStringNoEscape)
{
    TOMLTypeInfo info{ElementType::String, StringType::Literal};
    EXPECT_EQ(WriteValueToString(ValueType(std::string("no\\nescape")), info), R"('no\nescape')");
}

TEST(TOMLParserTest, WriteValue_MultiBasicString)
{
    TOMLTypeInfo info{ElementType::String, StringType::MultiBasic};
    EXPECT_EQ(WriteValueToString(ValueType(std::string("hello")), info), R"("""hello""")");
}

TEST(TOMLParserTest, WriteValue_MultiLiteralString)
{
    TOMLTypeInfo info{ElementType::String, StringType::MultiLiteral};
    EXPECT_EQ(WriteValueToString(ValueType(std::string("hello")), info), "'''hello'''");
}

TEST(TOMLParserTest, WriteValue_ArrayRecurses)
{
    std::vector<TOMLValue> arr{ TOMLValue(1), TOMLValue(2) };
    TOMLTypeInfo info{ElementType::Array, StringType::Invalid, { {ElementType::Int}, {ElementType::Int} }};
    EXPECT_EQ(WriteValueToString(ValueType(arr), info), "[1, 2]");
}

TEST(TOMLParserTest, WriteValue_NestedArrayRecurses)
{
    std::vector<TOMLValue> row0{ TOMLValue(1), TOMLValue(2) };
    std::vector<TOMLValue> row1{ TOMLValue(3), TOMLValue(4) };
    std::vector<TOMLValue> matrix{ TOMLValue(row0), TOMLValue(row1) };

    TOMLTypeInfo rowInfo{ElementType::Array, StringType::Invalid, { {ElementType::Int}, {ElementType::Int} }};
    TOMLTypeInfo info{ElementType::Array, StringType::Invalid, { rowInfo, rowInfo }};

    EXPECT_EQ(WriteValueToString(ValueType(matrix), info), "[[1, 2], [3, 4]]");
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

}

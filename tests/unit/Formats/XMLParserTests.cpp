#include <ASGE/Core/Formats/XML/XML.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using namespace asge::formats::xml;
using asge::errors::ConfError;

class XMLParserTest : public ::testing::Test
{
protected:
    std::filesystem::path m_XmlPath;

    void SetUp() override
    {
        auto const uniqueName = "asge_xml_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".xml";
        m_XmlPath = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_XmlPath, ec);
    }

    asge::Result<XMLNode> ParseText(std::string const& inContent) const
    {
        {
            std::ofstream file(m_XmlPath, std::ios::trunc);
            file << inContent;
        }
        return Parse(m_XmlPath);
    }
};

// ─── Success ──────────────────────────────────────────────────────────────────

TEST_F(XMLParserTest, Parse_SelfClosingRootWithAttributesSucceeds)
{
    auto result = ParseText("<root a=\"1\" b=\"two\"/>\n");
    ASSERT_TRUE(result.IsOk());

    auto const& root = result.Value();
    EXPECT_EQ(root.Name(), "root");
    ASSERT_NE(root.Find("a"), nullptr);
    EXPECT_EQ(root.Find("a")->Value(), "1");
    ASSERT_NE(root.Find("b"), nullptr);
    EXPECT_EQ(root.Find("b")->Value(), "two");
}

TEST_F(XMLParserTest, Parse_OpenAndCloseRootWithoutAttributesSucceeds)
{
    auto result = ParseText("<root>\n</root>\n");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().Name(), "root");
    EXPECT_FALSE(result.Value().Child(0).IsOk());
}

TEST_F(XMLParserTest, Parse_NestedChildrenKeepDocumentOrder)
{
    auto result = ParseText(
        "<root>\n"
        "  <first id=\"1\"/>\n"
        "  <second id=\"2\"/>\n"
        "  <third id=\"3\"/>\n"
        "  <fourth id=\"4\"/>\n"
        "  <fifth id=\"5\"/>\n"
        "</root>\n");
    ASSERT_TRUE(result.IsOk());

    char const* const names[] = { "first", "second", "third", "fourth", "fifth" };
    for (std::size_t i = 0; i < 5; ++i)
    {
        auto child = result.Value().Child(i);
        ASSERT_TRUE(child.IsOk()) << "child " << i;
        EXPECT_EQ(child.Value().get().Name(), names[i]);
        EXPECT_EQ(child.Value().get().Find("id")->Value(), std::to_string(i + 1));
    }
    EXPECT_FALSE(result.Value().Child(5).IsOk());
}

TEST_F(XMLParserTest, Parse_DeeplyNestedElementsAttachToCorrectParent)
{
    auto result = ParseText(
        "<a>\n"
        "  <b>\n"
        "    <c x=\"1\"/>\n"
        "  </b>\n"
        "  <d/>\n"
        "</a>\n");
    ASSERT_TRUE(result.IsOk());

    auto b = result.Value().Child(0);
    ASSERT_TRUE(b.IsOk());
    EXPECT_EQ(b.Value().get().Name(), "b");

    auto c = b.Value().get().Child(0);
    ASSERT_TRUE(c.IsOk());
    EXPECT_EQ(c.Value().get().Name(), "c");
    EXPECT_FALSE(b.Value().get().Child(1).IsOk());

    auto d = result.Value().Child(1);
    ASSERT_TRUE(d.IsOk());
    EXPECT_EQ(d.Value().get().Name(), "d");
}

TEST_F(XMLParserTest, Parse_DeclarationAndCommentsAreSkipped)
{
    auto result = ParseText(
        "<?xml version=\"1.0\"?>\n"
        "<!-- leading comment -->\n"
        "\n"
        "<root>\n"
        "  <child/> <!-- trailing comment -->\n"
        "</root>\n");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().Name(), "root");
    EXPECT_TRUE(result.Value().Child(0).IsOk());
}

TEST_F(XMLParserTest, Parse_AttributeValueMayContainSpaces)
{
    auto result = ParseText("<root title=\"hello big world\" n=\"3\"/>\n");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().Find("title")->Value(), "hello big world");
    EXPECT_EQ(result.Value().Find("n")->Value(), "3");
}

TEST_F(XMLParserTest, Parse_TypedAttributeAccessorsWork)
{
    auto result = ParseText("<root i=\"-4\" f=\"1.5\" b=\"true\"/>\n");
    ASSERT_TRUE(result.IsOk());

    auto const& root = result.Value();
    EXPECT_EQ(root.Find("i")->AsInt().Value(), -4);
    EXPECT_FLOAT_EQ(root.Find("f")->AsFloat().Value(), 1.5f);
    EXPECT_TRUE(root.Find("b")->AsBool().Value());
}

// ─── Child iteration ──────────────────────────────────────────────────────────

TEST_F(XMLParserTest, Iterate_NodeWithoutChildrenHasEmptyRange)
{
    auto result = ParseText("<root/>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode root = std::move(result).Value();
    EXPECT_EQ(root.begin(), root.end());
    EXPECT_EQ(root.cbegin(), root.cend());
}

TEST_F(XMLParserTest, Iterate_RangeForVisitsChildrenInDocumentOrder)
{
    auto result = ParseText(
        "<root>\n"
        "  <a/>\n"
        "  <b/>\n"
        "  <c/>\n"
        "</root>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode root = std::move(result).Value();
    std::string names;
    for (auto& child : root) names += child.Name();
    EXPECT_EQ(names, "abc");
}

TEST_F(XMLParserTest, Iterate_ConstIteratorsMatchChildCount)
{
    auto result = ParseText("<root>\n<a/>\n<b/>\n</root>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode const& root = result.Value();
    EXPECT_EQ(std::distance(root.cbegin(), root.cend()), 2);
    EXPECT_EQ(root.cbegin()->Name(), "a");
    EXPECT_EQ((root.cend() - 1)->Name(), "b");
}

TEST_F(XMLParserTest, Iterate_OnlyDirectChildrenAreVisited)
{
    auto result = ParseText(
        "<root>\n"
        "  <outer>\n"
        "    <inner/>\n"
        "  </outer>\n"
        "  <sibling/>\n"
        "</root>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode root = std::move(result).Value();
    std::string names;
    for (auto& child : root) names += child.Name() + ",";
    EXPECT_EQ(names, "outer,sibling,");
}

TEST_F(XMLParserTest, Iterate_NestedNodesCanBeIteratedRecursively)
{
    auto result = ParseText(
        "<root>\n"
        "  <a>\n"
        "    <a1/>\n"
        "    <a2/>\n"
        "  </a>\n"
        "  <b/>\n"
        "</root>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode root = std::move(result).Value();
    std::string names;
    for (auto& child : root)
    {
        names += child.Name() + ":";
        for (auto& grandChild : child) names += grandChild.Name() + ",";
    }
    EXPECT_EQ(names, "a:a1,a2,b:");
}

TEST_F(XMLParserTest, Iterate_ChildrenKeepTheirAttributes)
{
    auto result = ParseText(
        "<root>\n"
        "  <item id=\"1\"/>\n"
        "  <item id=\"2\"/>\n"
        "  <item id=\"3\"/>\n"
        "</root>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode root = std::move(result).Value();
    int sum = 0;
    for (auto& child : root)
    {
        auto id = child["id"];
        ASSERT_TRUE(id.IsOk());
        sum += id.Value().get().AsInt().Value();
    }
    EXPECT_EQ(sum, 6);
}

TEST_F(XMLParserTest, Iterate_AgreesWithChildIndexAccess)
{
    auto result = ParseText("<root>\n<a/>\n<b/>\n<c/>\n<d/>\n<e/>\n</root>\n");
    ASSERT_TRUE(result.IsOk());

    XMLNode root = std::move(result).Value();
    std::size_t idx = 0;
    for (auto& child : root)
    {
        auto byIndex = root.Child(idx++);
        ASSERT_TRUE(byIndex.IsOk());
        EXPECT_EQ(&byIndex.Value().get(), &child);
    }
    EXPECT_EQ(idx, 5u);
}

// ─── ToString / SaveToFile ────────────────────────────────────────────────────

TEST_F(XMLParserTest, ToString_WritesDeclarationAndSelfClosingRoot)
{
    XMLNode root("root");
    auto text = root.ToString();
    ASSERT_TRUE(text.IsOk());
    EXPECT_EQ(text.Value(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root/>\n");
}

TEST_F(XMLParserTest, ToString_IndentsChildrenAndSortsAttributes)
{
    XMLNode root("root");
    ASSERT_TRUE(root.AddAttribute("z", "1").IsOk());
    ASSERT_TRUE(root.AddAttribute("a", "two words").IsOk());
    auto& child = root.AddChild();
    child.SetName("child");
    auto& leaf = child.AddChild();
    leaf.SetName("leaf");

    auto text = root.ToString();
    ASSERT_TRUE(text.IsOk());
    EXPECT_EQ(text.Value(),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<root a=\"two words\" z=\"1\">\n"
        "  <child>\n"
        "    <leaf/>\n"
        "  </child>\n"
        "</root>\n");
}

TEST_F(XMLParserTest, ToString_ParseRoundTripPreservesTree)
{
    auto parsed = ParseText(
        "<root id=\"7\" label=\"a b\">\n"
        "  <one x=\"1\"/>\n"
        "  <two>\n"
        "    <three y=\"2\"/>\n"
        "  </two>\n"
        "</root>\n");
    ASSERT_TRUE(parsed.IsOk());

    auto text = parsed.Value().ToString();
    ASSERT_TRUE(text.IsOk());
    auto again = ParseText(text.Value());
    ASSERT_TRUE(again.IsOk());

    auto const& root = again.Value();
    EXPECT_EQ(root.Name(), "root");
    EXPECT_EQ(root.Find("id")->Value(), "7");
    EXPECT_EQ(root.Find("label")->Value(), "a b");
    auto two = root.Child(1);
    ASSERT_TRUE(two.IsOk());
    EXPECT_EQ(two.Value().get().Name(), "two");
    EXPECT_EQ(two.Value().get().Child(0).Value().get().Find("y")->Value(), "2");
    EXPECT_EQ(text.Value(), again.Value().ToString().Value());
}

TEST_F(XMLParserTest, ToString_EmptyNameReturnsBadFormat)
{
    auto text = XMLNode().ToString();
    ASSERT_FALSE(text.IsOk());
    EXPECT_EQ(text.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, ToString_NameWithReservedCharacterReturnsBadFormat)
{
    auto text = XMLNode("bad name").ToString();
    ASSERT_FALSE(text.IsOk());
    EXPECT_EQ(text.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, ToString_UnsupportedAttributeValuesReturnBadFormat)
{
    for (char const* value : { "", "say \"hi\"", "a<b", "a>b", "two\nlines" })
    {
        XMLNode root("root");
        ASSERT_TRUE(root.AddAttribute("v", value).IsOk());
        auto text = root.ToString();
        ASSERT_FALSE(text.IsOk()) << "value: " << value;
        EXPECT_EQ(text.Code(), make_error_code(ConfError::XmlBadFormat));
    }
}

TEST_F(XMLParserTest, ToString_BadChildErrorPropagates)
{
    XMLNode root("root");
    (void)root.AddChild(); // unnamed child
    auto text = root.ToString();
    ASSERT_FALSE(text.IsOk());
    EXPECT_EQ(text.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, ToString_NestingBeyondMaxDepthReturnsError)
{
    XMLNode root("n");
    XMLNode* cur = &root;
    for (int i = 0; i < 64; ++i)
    {
        cur = &cur->AddChild();
        cur->SetName("n");
    }

    auto text = root.ToString(); // 65 levels
    ASSERT_FALSE(text.IsOk());
    EXPECT_EQ(text.Code(), make_error_code(ConfError::XmlNestingTooDeep));
}

TEST_F(XMLParserTest, SaveToFile_WritesFileThatParsesBack)
{
    XMLNode root("root");
    ASSERT_TRUE(root.AddAttribute("k", "v").IsOk());
    root.AddChild().SetName("kid");

    ASSERT_TRUE(root.SaveToFile(m_XmlPath).IsOk());
    auto parsed = Parse(m_XmlPath);
    ASSERT_TRUE(parsed.IsOk());
    EXPECT_EQ(parsed.Value().Find("k")->Value(), "v");
    EXPECT_EQ(parsed.Value().Child(0).Value().get().Name(), "kid");
}

TEST_F(XMLParserTest, SaveToFile_InvalidNodeReturnsErrorAndWritesNothing)
{
    auto result = XMLNode().SaveToFile(m_XmlPath);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
    EXPECT_FALSE(std::filesystem::exists(m_XmlPath));
}

// ─── Attribute lookup ─────────────────────────────────────────────────────────

TEST_F(XMLParserTest, Lookup_MissingAttributeReturnsNotFound)
{
    auto result = ParseText("<root a=\"1\"/>\n");
    ASSERT_TRUE(result.IsOk());

    EXPECT_EQ(result.Value().Find("nope"), nullptr);
    auto attr = result.Value()["nope"];
    ASSERT_FALSE(attr.IsOk());
    EXPECT_EQ(attr.Code(), make_error_code(ConfError::XmlAttributeNotFound));
}

// ─── Error cases ──────────────────────────────────────────────────────────────

TEST_F(XMLParserTest, Parse_NonExistentFileReturnsError)
{
    auto result = Parse(m_XmlPath); // never written
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST_F(XMLParserTest, Parse_MismatchedClosingTagReturnsError)
{
    auto result = ParseText("<a>\n</b>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlNonMatchingClosingTag));
}

TEST_F(XMLParserTest, Parse_UnclosedElementReturnsBadFormat)
{
    auto result = ParseText("<a>\n  <b/>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_StrayClosingTagReturnsBadFormat)
{
    auto result = ParseText("</a>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_SecondRootElementReturnsBadFormat)
{
    auto result = ParseText("<a/>\n<b/>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_DuplicatedAttributeReturnsError)
{
    auto result = ParseText("<a x=\"1\" x=\"2\"/>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlDuplicatedAttribute));
}

TEST_F(XMLParserTest, Parse_NestingBeyondMaxDepthReturnsError)
{
    std::string text;
    for (int i = 0; i < 65; ++i) text += "<n>\n";
    for (int i = 0; i < 65; ++i) text += "</n>\n";

    auto result = ParseText(text);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlNestingTooDeep));
}

TEST_F(XMLParserTest, Parse_NestingAtMaxDepthSucceeds)
{
    std::string text;
    for (int i = 0; i < 64; ++i) text += "<n>\n";
    for (int i = 0; i < 64; ++i) text += "</n>\n";

    EXPECT_TRUE(ParseText(text).IsOk());
}

TEST_F(XMLParserTest, Parse_AttributeWithoutEqualSignReturnsBadFormat)
{
    auto result = ParseText("<a x/>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_AttributeWithoutQuotesReturnsBadFormat)
{
    auto result = ParseText("<a x=1/>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_EmptyAttributeValueReturnsBadFormat)
{
    auto result = ParseText("<a x=\"\"/>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_MissingClosingBracketReturnsBadFormat)
{
    auto result = ParseText("<a x=\"1\"\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

TEST_F(XMLParserTest, Parse_NonTagLineReturnsBadFormat)
{
    auto result = ParseText("<a>\nsome text\n</a>\n");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::XmlBadFormat));
}

} // namespace

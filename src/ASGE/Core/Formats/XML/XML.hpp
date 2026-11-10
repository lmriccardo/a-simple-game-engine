#pragma once

#include <cstdint>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileData.hpp>

namespace asge::formats::xml
{

/**
 * @brief Hasher enabling StringView lookups in a String-keyed map.
 *
 * Marked transparent so XMLNode can search its attributes by StringView
 * without building a temporary String for every lookup.
 */
struct TransparentHash
{
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(str::StringView inK) const noexcept;
};

/**
 * @brief A single `name="value"` attribute of an XML element.
 *
 * Stores the raw text of the value and converts it on demand through the
 * `As*` accessors. Conversion failures carry the attribute name and the
 * line it was declared on in the error detail.
 */
class XMLAttribute
{
    str::String   m_Value;  // Raw value text, without quotes
    std::uint32_t m_Line;   // Line in the XML file (0-based)
public:
    str::String m_Name;     // Attribute name

    XMLAttribute( str::StringView inName, str::StringView inValue, std::uint32_t inLine );

    /**
     * @brief Converts the value to a signed 32-bit integer.
     *
     * The whole value must be consumed: trailing characters or an
     * out-of-range number return an error instead of a truncated result.
     */
    [[nodiscard]] Result<std::int32_t>  AsInt     () const noexcept;

    /** @brief Converts the value to an unsigned 32-bit integer; same rules as AsInt(). */
    [[nodiscard]] Result<std::uint32_t> AsUInt    () const noexcept;

    /**
     * @brief Converts the value to a float.
     *
     * Trailing characters and non-finite results (inf, nan) are rejected,
     * on top of the usual parse failures.
     */
    [[nodiscard]] Result<float>         AsFloat   () const noexcept;

    /** @brief Converts the value to a double; same rules as AsFloat(). */
    [[nodiscard]] Result<double>        AsDouble  () const noexcept;

    /**
     * @brief Converts the value to a bool.
     *
     * Only the exact, lowercase strings `true` and `false` are accepted;
     * anything else (including `1`/`0`) returns an error.
     */
    [[nodiscard]] Result<bool>          AsBool    () const noexcept;

    /** @brief Converts the value to a signed 64-bit integer; same rules as AsInt(). */
    [[nodiscard]] Result<std::int64_t>  AsLong    () const noexcept;

    /** @brief Converts the value to an unsigned 64-bit integer; same rules as AsInt(). */
    [[nodiscard]] Result<std::uint64_t> AsULong   () const noexcept;

    /**
     * @brief Returns the raw value text.
     *
     * The view points into the attribute and is valid as long as it lives.
     */
    [[nodiscard]] str::StringView Value() const noexcept;
};

/**
 * @brief An XML element: a name, its attributes and its child elements.
 *
 * Nodes are normally produced by Parse() but can also be built by hand with
 * SetName(), AddAttribute() and AddChild(). Children are stored by value,
 * in document order, and can be reached by index or by iteration.
 */
class XMLNode
{
    using XMLAttributeMap = std::unordered_map<str::String, XMLAttribute,
        TransparentHash, std::equal_to<>>;

    str::String             m_Name      {}; // Element name
    XMLAttributeMap         m_Attrs     {}; // Attributes by name
    std::vector<XMLNode>    m_Children  {}; // Child elements, document order
    std::uint32_t           m_Line      {}; // Line of the opening tag

public:
    using attribute_reference = std::reference_wrapper<XMLAttribute const>;
    using node_reference = std::reference_wrapper<XMLNode const>;

    XMLNode() = default;
    XMLNode( str::StringView inName, std::uint32_t inLine = 0 );

    XMLNode( XMLNode const& ) = default;
    XMLNode( XMLNode && ) = default;
    XMLNode& operator=( XMLNode const& ) = default;
    XMLNode& operator=( XMLNode && ) = default;

    /**
     * @brief Sets the element name and the line it was declared on.
     *
     * The line is also attached to every attribute added afterwards, so
     * it ends up in their conversion error messages.
     */
    void SetName( str::StringView inName, std::uint32_t inLine = 0 );

    /**
     * @brief Adds an attribute to this element.
     *
     * Names are unique per element: adding one that already exists fails
     * with `XmlDuplicatedAttribute` and leaves the existing attribute
     * untouched.
     */
    [[nodiscard]] BoolResult AddAttribute( str::StringView inAttrName, str::StringView inAttrValue );

    /**
     * @brief Appends an empty child and returns a reference to it.
     *
     * The reference is invalidated by the next AddChild() on this node,
     * since children live in a vector. Fill the child before adding more.
     */
    [[nodiscard]] XMLNode& AddChild();

    /**
     * @brief Looks up an attribute by name.
     *
     * Returns `XmlAttributeNotFound` when it does not exist. Use Find()
     * when a missing attribute is not an error.
     */
    Result<attribute_reference> operator[]( str::StringView inAttrName ) const noexcept;

    /** @brief Returns the attribute with the given name, or nullptr if missing. */
    XMLAttribute const* Find( str::StringView inAttrName ) const noexcept;

    /** @brief Returns the element name; empty for a default-constructed node. */
    str::String const& Name() const noexcept;

    /**
     * @brief Returns the child at the given position, in document order.
     *
     * Fails with `std::errc::invalid_argument` when the index is not
     * smaller than the number of children.
     */
    Result<node_reference> Child( std::size_t inChildIdx ) const noexcept;

    /** @brief Iterator to the first child; enables range-for over a node. */
    std::vector<XMLNode>::iterator begin() noexcept;

    /** @brief Const iterator to the first child; enables range-for over a const node. */
    std::vector<XMLNode>::const_iterator begin() const noexcept;

    /** @brief Past-the-end iterator over the children. */
    std::vector<XMLNode>::iterator end  () noexcept;

    /** @brief Const past-the-end iterator over the children. */
    std::vector<XMLNode>::const_iterator end() const noexcept;

    /** @brief Const iterator to the first child. */
    std::vector<XMLNode>::const_iterator cbegin() const noexcept;

    /** @brief Const past-the-end iterator over the children. */
    std::vector<XMLNode>::const_iterator cend  () const noexcept;

    /**
     * @brief Serializes this node and its subtree to XML text.
     *
     * Starts with an `<?xml ?>` declaration and writes one tag per line,
     * indented by two spaces, in the format Parse() reads back. Attributes
     * are sorted by name for stable output, and childless nodes are written
     * self-closing. Fails with `XmlBadFormat` for names with reserved
     * characters and for empty values or values containing `"`, `<`, `>` or
     * a line break (the parser has no escaping), and with `XmlNestingTooDeep`
     * past 64 levels.
     */
    [[nodiscard]] Result<str::String> ToString() const;

    /**
     * @brief Writes ToString() to a file, replacing its content.
     *
     * Serialization errors are returned as-is and the file is not touched
     * in that case. I/O failures come from the underlying filesystem call.
     */
    [[nodiscard]] BoolResult SaveToFile( filesystem::Path const& inPath ) const;

private:
    BoolResult Serialize( str::String& outText, unsigned inDepth ) const; // Appends this subtree to outText
};

/**
 * @brief Parses an XML file into a tree of nodes and returns its root.
 *
 * A small, line-based parser: each tag sits on its own line, attribute
 * values are double-quoted and non-empty, and text content is not
 * supported. The `<?xml ?>` declaration and comments are skipped, and
 * nesting is limited to 64 levels. A missing file returns
 * `std::errc::no_such_file_or_directory`; malformed input returns the
 * matching `Xml*` ConfError with the offending line in the detail.
 */
[[nodiscard]] Result<XMLNode> Parse( filesystem::Path const& inPath ) noexcept;

}

#pragma once

#include <ASGE/Core/Media/Font.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>

namespace asge::game::components
{

/**
 * @brief How Draw(UILabel) positions text within its UIRect's height.
 *
 * Top sits the text at the rect's own top edge, same as before this existed.
 */
enum class VerticalAlign
{
    Top = 0,
    Center,
    Bottom
};

/**
 * @brief Converts a VerticalAlign to its lowercase TOML/display name.
 *
 * Gives "top", "center" or "bottom", and "top" for anything unrecognized.
 */
str::String ToString( VerticalAlign inAlign ) noexcept;

/**
 * @brief The read-side counterpart to ToString.
 *
 * Any string other than "top"/"center"/"bottom" maps to Top.
 */
VerticalAlign FromString( str::StringView inStr ) noexcept;

/**
 * @brief A text label drawn with a baked Font atlas.
 *
 * m_FontPath is resolved into m_Font by asset::Resolver<UILabel> (see its
 * own doc comment) via AssetManager::GetFont, re-resolving whenever it
 * differs from m_ResolvedFontPath. m_Font is a non-owning pointer into
 * AssetManager's own font pool, which never evicts entries -- the same
 * guarantee Sprite::m_Texture relies on for AssetManager's texture cache.
 */
struct UILabel
{
    // Serialized
    str::String             m_FontPath          {};                         // VFS path of the font file
    str::String             m_Text              {"Placeholder"};            // The text shown
    str::TextAlign          m_Align             {str::TextAlign::Left};     // Horizontal alignment within the rect
    VerticalAlign           m_VerticalAlign     {VerticalAlign::Center};    // Vertical alignment within the rect
    graphics::RGBA_Color    m_Color             {graphics::colors::s_Black};// Text color
    int                     m_FontPixelHeight   {16};                       // Pixel height the font is baked at
    bool                    m_AutoSize          {true};                     // Size the rect to fit the text
    bool                    m_WordWrap          {false}; // wraps to the rect's width; needs m_AutoSize off

    // Resolved via Asset Resolution
    media::Font const*  m_Font{nullptr};          // Resolved font, non-owning; null until ResolveAssets runs
    video::ITexture*    m_Texture{nullptr};       // The font's glyph atlas texture, non-owning
    str::String         m_ResolvedFontPath{};     // The path m_Font was last resolved from
    int                 m_ResolvedFontPixelHeight{}; // The bake height m_Font was last resolved at
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UILabel>
{
    static constexpr str::StringView kTableName = "UILabel"; // Name of the TOML subtable (de)serialization uses

    using T = components::UILabel;

    /**
     * @brief Writes the component's persisted fields into its TOML subtable.
     *
     * Part of the Serializer contract; the struct's own comment lists exactly
     * which fields are saved.
     */
    static void ToToml(
                            T inValue,
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    /**
     * @brief Rebuilds the component from its TOML subtable.
     *
     * Fields that are runtime-only are left at their defaults; see the
     * struct's own comment for which ones are read.
     */
    static T FromToml(
                            asge::formats::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
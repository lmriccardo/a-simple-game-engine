#pragma once

#include <ASGE/Core/Media/Font.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Graphics/Color.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>

namespace asge::game::components
{

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
    str::String             m_FontPath          {};
    str::String             m_Text              {"Placeholder"};
    str::TextAlign          m_Align             {str::TextAlign::Left};
    graphics::RGBA_Color    m_Color             {graphics::colors::s_Black};
    int                     m_FontPixelHeight   {16};
    bool                    m_AutoSize          {true};

    // Revoled via Asset Resolution
    media::Font const*  m_Font{nullptr};
    video::ITexture*    m_Texture{nullptr};
    str::String         m_ResolvedFontPath{};
};

}

namespace asge::game::scene
{

template<>
struct Serializer<components::UILabel>
{
    static constexpr str::StringView kTableName = "UILabel";

    using T = components::UILabel;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
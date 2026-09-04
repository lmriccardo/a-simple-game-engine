#include "Font.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace
{
constexpr int kFirstChar = 32;
constexpr int kNumChars  = 95; // ASCII 32..126 inclusive
constexpr int kAtlasW    = 512;
constexpr int kAtlasH    = 512;
}

asge::media::Font::Font(
    std::unordered_map<char32_t, GlyphMetrics> inGlyphs, Image inAtlasImage, 
    int inLineHeight, int inAscent, int inDescent
) noexcept
: m_Glyphs( std::move(inGlyphs) )
, m_AtlasImage( std::move(inAtlasImage) )
, m_LineHeight( inLineHeight )
, m_Ascent(inAscent)
, m_Descent(inDescent)
{}

asge::Result<asge::media::Font> asge::media::Font::Load(const filesystem::Path &inPath, int inPixelHeight)
{
    auto binBuffer = filesystem::ReadBinary( inPath );
    if ( !binBuffer ) return Result<Font>::Err(binBuffer.Error());

    auto const& fileBytes = binBuffer.Value();
    auto const* fontData = reinterpret_cast<unsigned char const*>(fileBytes.data());

    // Parse the font's internal tables (glyf/loca/cmap/...); only needed
    // here to query vertical metrics below -- baking below doesn't use it.
    //
    // stbtt_GetFontOffsetForIndex validates the first 4 bytes against known
    // sfnt tags before anything else touches the buffer, returning -1 for
    // non-font data -- required here, not optional: stbtt_InitFont has no
    // buffer-length parameter and does no bounds checking of its own, so
    // calling it with a hardcoded offset of 0 on malformed/truncated input
    // reads past the buffer instead of failing cleanly (confirmed via a
    // crash on garbage input before this fix).
    int const fontOffset = stbtt_GetFontOffsetForIndex( fontData, 0 );
    stbtt_fontinfo fontInfo;
    if ( fontOffset < 0 || !stbtt_InitFont( &fontInfo, fontData, fontOffset ) )
    {
        auto const ec = make_error_code(errors::FontError::InitFailed);
        return Result<Font>::Err(ec, str::ToUTF8(inPath.u8string()));
    }

    // Bake ASCII 32-126 into a single atlas bitmap. bakedChars is filled
    // with each glyph's atlas rect + pen offsets in the same pass.
    Image::data_t atlasPixels( static_cast<std::size_t>(kAtlasW) * static_cast<std::size_t>(kAtlasH) );
    std::vector<stbtt_bakedchar> bakedChars(static_cast<std::size_t>(kNumChars));

    const int bakedResult = stbtt_BakeFontBitmap( 
        fontData, 0, static_cast<float>(inPixelHeight), atlasPixels.data(), 
        kAtlasW, kAtlasH, kFirstChar, kNumChars, bakedChars.data()
    );

    // <= 0 means the atlas was too small to fit the requested range.
    if ( bakedResult <= 0 )
    {
        auto const ec = make_error_code(errors::FontError::BakeFailed);
        return Result<Font>::Err(ec, str::ToUTF8(inPath.u8string()));
    }

    // Convert stb's per-glyph bake data into our own GlyphMetrics, keyed
    // by codepoint so DrawString can look glyphs up directly later.
    std::unordered_map<char32_t, GlyphMetrics> glyphs;
    glyphs.reserve(static_cast<std::size_t>(kNumChars));

    for ( int i = 0; i < kNumChars; ++i )
    {
        stbtt_bakedchar const& baked = bakedChars[static_cast<std::size_t>(i)];
        GlyphMetrics metrics{};
        metrics.uv_rect = math::Rect{
            static_cast<float>(baked.x0), static_cast<float>(baked.y0),
            static_cast<float>(baked.x1 - baked.x0), static_cast<float>(baked.y1 - baked.y0)
        };

        metrics.size = math::Int2{
            static_cast<int>(baked.x1 - baked.x0), static_cast<int>(baked.y1 - baked.y0)
        };

        metrics.bearing = math::Int2{
            static_cast<int>(baked.xoff), static_cast<int>(baked.yoff)
        };

        metrics.advance = static_cast<int>(baked.xadvance);
        glyphs.emplace(static_cast<char32_t>(kFirstChar + i), metrics);
    }

    // Font-wide vertical metrics, scaled from font units to pixels at
    // the requested bake size -- used for line spacing in text layout.
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    float const scale = stbtt_ScaleForPixelHeight(&fontInfo, static_cast<float>(inPixelHeight));

    int const scaledAscent   = static_cast<int>(static_cast<float>(ascent) * scale);
    int const scaledDescent  = static_cast<int>(static_cast<float>(descent) * scale);
    int const scaledLineGap  = static_cast<int>(static_cast<float>(lineGap) * scale);
    int const lineHeight     = scaledAscent - scaledDescent + scaledLineGap;

    // Wrap the baked bitmap in an Image so it flows through the existing
    // Image -> ITexture upload path, same as any other texture.
    Image atlasImage(kAtlasW, kAtlasH, PixelFormat::A8, atlasPixels);
    return Result<Font>::Ok( Font( std::move(glyphs), std::move(atlasImage),
        lineHeight, scaledAscent, scaledDescent ) 
    );
}

asge::Result<asge::media::GlyphMetrics> asge::media::Font::GetGlyph(char32_t inCodepoint) const
{
    auto it = m_Glyphs.find( inCodepoint );
    if ( it == m_Glyphs.end() ) return Result<GlyphMetrics>::Err(
        make_error_code( errors::FontError::UnexistingCodepoint ), 
        "Codepoint " + std::to_string(inCodepoint) 
    );

    return Result<GlyphMetrics>::Ok( it->second );
}

const asge::media::Image &asge::media::Font::GetAtlasImage() const noexcept
{
    return m_AtlasImage;
}

int asge::media::Font::GetLineHeight() const noexcept
{
    return m_LineHeight;
}

int asge::media::Font::GetAscent() const noexcept
{
    return m_Ascent;
}

int asge::media::Font::GetDescent() const noexcept
{
    return m_Descent;
}

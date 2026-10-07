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

asge::math::Int2 asge::media::Font::GetAtlasSize() noexcept
{
    return { kAtlasW, kAtlasH };
}

asge::Result<int> asge::media::Font::FindMaxPixelHeight(
    const filesystem::Path& inPath, int inUpperBound )
{
    // 1px first, on its own -- a failure here is the font itself being
    // invalid (bad/corrupt data), not "too big to fit", so it's propagated
    // as-is rather than folded into the search below.
    auto const smallest = Load( inPath, 1 );
    if ( !smallest ) return Result<int>::Err( smallest.Error() );

    // Binary search over Load itself as the oracle -- exact, not a
    // heuristic (see this function's own doc comment for why there's no
    // direct formula). Biased high so it converges on the largest height
    // that still fits, not the smallest that doesn't.
    int lo = 1, hi = inUpperBound;
    while ( lo < hi )
    {
        int const mid = lo + ( hi - lo + 1 ) / 2;
        if ( Load( inPath, mid ) ) lo = mid; else hi = mid - 1;
    }

    return Result<int>::Ok( lo );
}

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
        return Result<Font>::Err(ec, str::ToUtf8(inPath.u8string()));
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
        return Result<Font>::Err(ec, str::ToUtf8(inPath.u8string()));
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
    Image atlasImage(kAtlasW, kAtlasH, graphics::PixelFormat::A8, atlasPixels);
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

asge::math::Float2 asge::media::Font::Measure(str::StringView inText) const noexcept
{
    float lineWidth = 0.0f, maxWidth = 0.0f;
    int lines = 1;

    // No ++pos here -- DecodeUtf8 already advances pos (by reference) past
    // however many bytes the codepoint it just decoded took; incrementing
    // it again here would double-advance and silently skip every other
    // character.
    for ( std::size_t pos = 0; pos < inText.size(); )
    {
        char32_t const cp = str::DecodeUtf8( inText, pos );
        if ( cp == '\n' )
        {
            maxWidth = std::max( maxWidth, lineWidth );
            lineWidth = 0.0f;
            ++lines;
            continue;
        }

        auto const it = m_Glyphs.find( cp );
        if ( it == m_Glyphs.end() ) continue;
        lineWidth += static_cast<float>( it->second.advance );
    }

    maxWidth = std::max( maxWidth, lineWidth );
    float const height = static_cast<float>( 
        ( lines - 1 ) * m_LineHeight + ( m_Ascent - m_Descent ) );
    
    return { maxWidth, height };
}

asge::str::String asge::media::Font::WrapText( str::StringView inText, float inMaxWidth ) const noexcept
{
    str::String out;
    float const spaceWidth = Measure( " " ).x();
    float lineWidth = 0.0f;

    // ponytail: runs of spaces collapse to one, no mid-word breaking; add both if a use case needs them
    for ( std::size_t pos = 0; pos < inText.size(); )
    {
        if ( inText[pos] == '\n' ) { out += '\n'; lineWidth = 0.0f; ++pos; continue; }
        if ( inText[pos] == ' ' )   { ++pos; continue; }

        std::size_t end = inText.find_first_of( " \n", pos );
        if ( end == str::StringView::npos ) end = inText.size();

        auto const word = inText.substr( pos, end - pos );
        float const wordWidth = Measure( word ).x();

        if ( lineWidth > 0.0f )
        {
            if ( lineWidth + spaceWidth + wordWidth > inMaxWidth ) { out += '\n'; lineWidth = 0.0f; }
            else { out += ' '; lineWidth += spaceWidth; }
        }

        out += word;
        lineWidth += wordWidth;
        pos = end;
    }

    return out;
}
